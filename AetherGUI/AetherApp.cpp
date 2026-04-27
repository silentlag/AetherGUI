#include "AetherApp.h"

#include <SetupAPI.h>
#include <hidsdi.h>
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

static bool FileExists(const std::string& path) {
	DWORD attrs = GetFileAttributesA(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string WideToUtf8(const std::wstring& text) {
	if (text.empty())
		return std::string();

	int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (size <= 1)
		return std::string();

	std::string result(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], size, nullptr, nullptr);
	if (!result.empty() && result.back() == '\0')
		result.pop_back();
	return result;
}

static std::wstring Ellipsize(const std::wstring& text, size_t maxChars) {
	if (text.length() <= maxChars)
		return text;
	if (maxChars <= 3)
		return text.substr(0, maxChars);
	return text.substr(0, maxChars - 3) + L"...";
}

struct MonitorEnumContext {
	std::vector<AetherApp::DisplayTarget>* targets = nullptr;
	int virtualLeft = 0;
	int virtualTop = 0;
	int index = 1;
};

static BOOL CALLBACK EnumDisplayTargetProc(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
	MonitorEnumContext* ctx = reinterpret_cast<MonitorEnumContext*>(data);
	MONITORINFOEXW mi = {};
	mi.cbSize = sizeof(mi);
	if (!GetMonitorInfoW(monitor, &mi))
		return TRUE;

	AetherApp::DisplayTarget target;
	target.x = (float)(mi.rcMonitor.left - ctx->virtualLeft);
	target.y = (float)(mi.rcMonitor.top - ctx->virtualTop);
	target.width = (float)(mi.rcMonitor.right - mi.rcMonitor.left);
	target.height = (float)(mi.rcMonitor.bottom - mi.rcMonitor.top);

	wchar_t label[96];
	swprintf_s(label, L"Monitor %d  %.0fx%.0f  X%+.0f Y%+.0f",
		ctx->index, target.width, target.height, target.x, target.y);
	target.label = label;

	ctx->targets->push_back(target);
	ctx->index++;
	return TRUE;
}

bool AetherApp::Initialize(HWND hwnd) {
	hWnd = hwnd;
	if (!renderer.Initialize(hwnd)) return false;

	RECT rc;
	GetClientRect(hwnd, &rc);
	OnResize((UINT)(rc.right - rc.left), (UINT)(rc.bottom - rc.top));

	lastFrameTime = std::chrono::high_resolution_clock::now();

	sidebar.x = 0;
	sidebar.y = Theme::Size::HeaderHeight;
	sidebar.AddTab(L"Area", L"\xE774");
	sidebar.AddTab(L"Filters", L"\xE71C");
	sidebar.AddTab(L"Settings", L"\xE713");
	sidebar.AddTab(L"Console", L"\xE756");
	sidebar.AddTab(L"About", L"\xE946");

	char exePath[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);
	std::string exeDir(exePath);
	size_t lastSlash = exeDir.find_last_of('\\');
	if (lastSlash != std::string::npos) {
		exeDir = exeDir.substr(0, lastSlash + 1);
	}
	servicePath = exeDir + "AetherService.exe";
	std::string serviceCandidates[] = {
		servicePath,
		exeDir + "..\\..\\x64\\Release\\AetherService.exe",
		exeDir + "..\\..\\AetherService\\x64\\Release\\AetherService.exe",
		exeDir + "..\\..\\..\\x64\\Release\\AetherService.exe",
		exeDir + "..\\..\\..\\AetherService\\x64\\Release\\AetherService.exe",
		exeDir + "..\\x64\\Release\\AetherService.exe"
	};
	for (const auto& candidate : serviceCandidates) {
		if (FileExists(candidate)) {
			servicePath = candidate;
			break;
		}
	}

	startStopBtn.Layout(0, 0, 100, 30, L"Start", true);

	srand((unsigned)GetTickCount());
	for (int i = 0; i < MAX_STARS; i++) stars[i].active = false;
	starSpawnTimer = 1.0f + (rand() % 300) / 100.0f;

	RefreshDetectedScreen();

	InitControls();
	AutoLoadConfig();

	// Check if VMulti driver is installed
	vmultiCheckDone = true;
	{
		GUID hidGuid;
		HidD_GetHidGuid(&hidGuid);
		HDEVINFO devInfo = SetupDiGetClassDevs(&hidGuid, NULL, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
		vmultiInstalled = false;
		if (devInfo != INVALID_HANDLE_VALUE) {
			SP_DEVICE_INTERFACE_DATA ifData;
			ifData.cbSize = sizeof(ifData);
			for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(devInfo, NULL, &hidGuid, idx, &ifData); idx++) {
				DWORD size = 0;
				SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, NULL, 0, &size, NULL);
				if (size > 0) {
					auto detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)malloc(size);
					detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
					if (SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, detail, size, NULL, NULL)) {
						HANDLE h = CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
						if (h != INVALID_HANDLE_VALUE) {
							HIDD_ATTRIBUTES attr = {};
							attr.Size = sizeof(attr);
							if (HidD_GetAttributes(h, &attr) && attr.VendorID == 0x00FF && attr.ProductID == 0xBACC) {
								vmultiInstalled = true;
							}
							CloseHandle(h);
						}
					}
					free(detail);
				}
				if (vmultiInstalled) break;
			}
			SetupDiDestroyDeviceInfoList(devInfo);
		}
	}

	autoStartAttempted = true;
	if (vmultiInstalled) {
		StartDriverService();
	}
	return true;
}

void AetherApp::Shutdown() {
	driver.Stop();
	renderer.Shutdown();
}

void AetherApp::InitControls() {
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2;
	float hw = (cw - Theme::Size::Padding) * 0.5f;

	area.tabletWidth.Layout(cx, 0, hw, L"Tablet Width (mm)", 1, 300, 76.2f, L"Width of the active tablet area in mm");
	area.tabletHeight.Layout(cx, 0, hw, L"Tablet Height (mm)", 1, 200, 47.625f, L"Height of the active tablet area in mm");
	area.tabletX.Layout(cx, 0, hw, L"Center X (mm)", 0.01f, 200, 76.0f, L"Horizontal center position of the area");
	area.tabletY.Layout(cx, 0, hw, L"Center Y (mm)", 0.01f, 200, 47.5f, L"Vertical center position of the area");
	area.screenWidth.Layout(cx + hw + Theme::Size::Padding, 0, hw, L"Screen Width (px)", 100, 7680, detectedScreenW, L"Width of the screen mapping area in pixels");
	area.screenHeight.Layout(cx + hw + Theme::Size::Padding, 0, hw, L"Screen Height (px)", 100, 4320, detectedScreenH, L"Height of the screen mapping area in pixels");
	area.screenX.Layout(cx + hw + Theme::Size::Padding, 0, hw, L"Screen X", 0, 7680, 0, L"Horizontal offset for multi-monitor setups");
	area.screenY.Layout(cx + hw + Theme::Size::Padding, 0, hw, L"Screen Y", 0, 4320, 0, L"Vertical offset for multi-monitor setups");
	area.rotation.Layout(cx, 0, cw, L"Rotation", -180, 180, 0, L"Rotate the tablet area in degrees");
	area.customValues.Layout(cx, 0, L"Custom Values", L"Show advanced position controls");
	area.customValues.value = false;
	area.lockAspect.Layout(cx, 0, L"Lock Aspect Ratio", L"Keep tablet area proportional to screen");

	filters.smoothingEnabled.Layout(cx, 0, L"Smoothing", L"Low-pass filter to reduce cursor jitter");
	filters.smoothingLatency.Layout(cx, 0, hw, L"Latency (ms)", 0, 100, 2, L"Time to reach target position. Higher = smoother but more lag");
	filters.smoothingInterval.Layout(cx + hw + 16, 0, hw, L"Interval (ms)", 1, 16, 2, L"Timer tick interval. Lower = smoother updates");

	filters.antichatterEnabled.Layout(cx, 0, L"Antichatter", L"Adaptive smoothing that increases near stationary positions");
	filters.antichatterStrength.Layout(cx, 0, hw, L"Strength", 1, 10, 3, L"Power curve exponent. Higher = more smoothing at rest");
	filters.antichatterMultiplier.Layout(cx + hw + 16, 0, hw, L"Multiplier", 1, 1000, 1, L"Scale factor for the distance weight");
	filters.antichatterOffsetX.Layout(cx, 0, hw, L"Offset X", -2, 5, 0, L"Distance offset for the weight formula");
	filters.antichatterOffsetY.Layout(cx + hw + 16, 0, hw, L"Offset Y", -1, 10, 0, L"Weight offset floor. Negative = allow zero weight");

	filters.noiseEnabled.Layout(cx, 0, L"Noise Reduction", L"Average recent positions to smooth out sensor noise");
	filters.noiseBuffer.Layout(cx, 0, hw, L"Buffer (packets)", 1, 50, 10, L"Number of recent positions to average. Higher = smoother but more lag");
	filters.noiseBuffer.format = L"%.0f";
	filters.noiseThreshold.Layout(cx + hw + 16, 0, hw, L"Threshold (mm)", 0, 20, 0.5f, L"Ignore movements smaller than this distance");
	filters.noiseIterations.Layout(cx, 0, hw, L"Iterations", 1, 50, 10, L"Number of averaging passes. More = heavier smoothing");
	filters.noiseIterations.format = L"%.0f";

	filters.velCurveEnabled.Layout(cx, 0, L"Prediction", L"Extrapolate cursor position based on velocity to reduce perceived lag");
	filters.velCurveMinSpeed.Layout(cx, 0, hw, L"Offset X", 0, 40, 3, L"Distance offset for the prediction curve");
	filters.velCurveMaxSpeed.Layout(cx + hw + 16, 0, hw, L"Offset Y", 0, 5, 0.3f, L"Base prediction amount added at all speeds");
	filters.velCurveSmoothing.Layout(cx, 0, hw, L"Strength", 0, 10, 1.1f, L"How far ahead to predict. Higher = more compensation");
	filters.velCurveSharpness.Layout(cx + hw + 16, 0, hw, L"Sharpness", 0.1f, 5, 1.0f, L"How quickly prediction ramps up with speed");

	filters.snapEnabled.Layout(cx, 0, L"Position Snap", L"Eliminate micro-jitter when the cursor is nearly stationary");
	filters.snapRadius.Layout(cx, 0, hw, L"Snap Radius (px)", 0.1f, 10, 0.5f, L"Movement below this threshold is suppressed");
	filters.snapSmooth.Layout(cx + hw + 16, 0, hw, L"Smoothness", 0, 1, 0.3f, L"How gradually snapping transitions. 0 = hard snap, 1 = very soft");

	filters.reconstructorEnabled.Layout(cx, 0, L"Reconstructor", L"Compensate for tablet hardware processing delay using velocity prediction. High values at your own risk!");
	filters.reconStrength.Layout(cx, 0, hw, L"Strength", 0, 2, 0.5f, L"How aggressively to compensate latency. 0 = off, 1 = full");
	filters.reconVelSmooth.Layout(cx + hw + 16, 0, hw, L"Vel. Smoothing", 0, 0.99f, 0.6f, L"Smoothing on velocity estimation. Higher = more stable but slower");
	filters.reconAccelCap.Layout(cx, 0, hw, L"Accel Cap", 1, 200, 50, L"Maximum acceleration magnitude to prevent overshoot on direction changes");
	filters.reconPredTime.Layout(cx + hw + 16, 0, hw, L"Pred. Time (ms)", 0.1f, 50, 5, L"How far ahead to extrapolate position in milliseconds");

	filters.adaptiveEnabled.Layout(cx, 0, L"Adaptive Filter", L"Statistical filter that balances prediction and measurement for optimal smoothing");
	filters.adaptiveProcessNoise.Layout(cx, 0, hw, L"Process Noise (Q)", 0.001f, 10, 0.02f, L"Expected movement variance. Higher = trusts measurements more (less smooth)");
	filters.adaptiveMeasNoise.Layout(cx + hw + 16, 0, hw, L"Meas. Noise (R)", 0.001f, 50, 0.5f, L"Expected sensor noise. Higher = trusts prediction more (smoother)");
	filters.adaptiveVelWeight.Layout(cx, 0, hw, L"Velocity Weight", 0, 5, 1, L"How much velocity influences the prediction model");

	outputMode.AddOption(L"Absolute");
	outputMode.AddOption(L"Relative");
	outputMode.AddOption(L"Windows Ink");
	outputMode.selected = 0;

	const wchar_t* btnOpts[] = { L"Disable", L"Mouse 1", L"Mouse 2", L"Mouse 3", L"Mouse 4", L"Mouse 5", L"Mouse Wheel" };
	buttonTip.Layout(cx, 0, hw, L"Pen Tip", L"Action when pen tip touches the tablet");
	buttonBottom.Layout(cx, 0, hw, L"Bottom Button", L"Action for the lower pen barrel button");
	buttonTop.Layout(cx, 0, hw, L"Top Button", L"Action for the upper pen barrel button");
	for (int i = 0; i < 7; i++) {
		buttonTip.AddOption(btnOpts[i]);
		buttonBottom.AddOption(btnOpts[i]);
		buttonTop.AddOption(btnOpts[i]);
	}
	buttonTip.selected = 1;
	buttonBottom.selected = 2;
	buttonTop.selected = 3;

	forceFullArea.Layout(cx, 0, L"Force Full Area", L"Use entire tablet surface");
	areaClipping.Layout(cx, 0, L"Area Clipping", L"Clip cursor to screen area bounds");
	areaClipping.value = true;
	areaLimiting.Layout(cx, 0, L"Area Limiting", L"Block input outside tablet area entirely");
	areaLimiting.value = false;

	tipThreshold.Layout(cx, 0, hw, L"Tip Threshold (%)", 0, 100, 1, L"Pressure needed to register a click. 0 = any touch, higher = harder press");
	tipThreshold.format = L"%.0f";

	overclockEnabled.Layout(cx, 0, L"Overclock", L"Boost driver timer rate for smoother cursor movement");
	overclockHz.Layout(cx, 0, hw, L"Target Rate (Hz)", 125, 2000, 1000, L"Target timer frequency. Higher = smoother. Actual rate depends on USB polling");
	overclockHz.format = L"%.0f";

	saveConfigBtn.Layout(0, 0, 100, 28, L"Save Config", false, L"Save all current settings to disk");
	loadConfigBtn.Layout(0, 0, 100, 28, L"Load Config", false, L"Reload settings from the saved config file");

	consoleInput.Layout(cx, 0, cw, L"Type command...");

	accentPicker.Layout(cx, 0, cw * 0.5f);
	accentPicker.SetRGB(Theme::Custom::AccentR, Theme::Custom::AccentG, Theme::Custom::AccentB);

	// === Aether Smooth filter controls ===
	aether.enabled.Layout(cx, 0, L"Aether Smooth", L"Adaptive multi-stage filter pipeline");
	aether.lagRemovalEnabled.Layout(cx, 0, L"Lag Removal", L"Counteract internal tablet processing delay");
	aether.lagRemovalStrength.Layout(cx, 0, hw, L"Strength", 0.1f, 2.0f, 0.6f, L"Lower = more aggressive, Higher = smoother");
	aether.stabilizerEnabled.Layout(cx, 0, L"Stabilizer", L"Velocity-adaptive smoothing (Adaptive Flow)");
	aether.stabilizerStability.Layout(cx, 0, hw, L"Stability", 0.01f, 10.0f, 1.0f, L"Smoothing at low speeds");
	aether.stabilizerSensitivity.Layout(cx + hw + 16, 0, hw, L"Sensitivity", 0.001f, 0.1f, 0.015f, L"Response to fast movement");
	aether.stabilizerSensitivity.format = L"%.4f";
	aether.snappingEnabled.Layout(cx, 0, L"Dynamic Snapping", L"Zero lag during fast aim snaps");
	aether.snappingInner.Layout(cx, 0, hw, L"Snap Radius (Min)", 0.0f, 5.0f, 0.5f, L"Area where smoothing is strongest");
	aether.snappingOuter.Layout(cx + hw + 16, 0, hw, L"Snap Radius (Max)", 0.5f, 20.0f, 3.0f, L"Beyond this smoothing is disabled");
	aether.suppressionEnabled.Layout(cx, 0, L"Suppression", L"Lock cursor when below threshold");
	aether.suppressionTime.Layout(cx, 0, hw, L"Time (ms)", 0.0f, 50.0f, 5.0f, L"Jitter suppression window");

	// === Full UI Themes — store defaults + make mutable copies ===
	uiThemeCount = 12;
	uiThemeDefaults[0]  = &Theme::Themes::Midnight;
	uiThemeDefaults[1]  = &Theme::Themes::Abyss;
	uiThemeDefaults[2]  = &Theme::Themes::Nord;
	uiThemeDefaults[3]  = &Theme::Themes::Void;
	uiThemeDefaults[4]  = &Theme::Themes::Rose;
	uiThemeDefaults[5]  = &Theme::Themes::Ember;
	uiThemeDefaults[6]  = &Theme::Themes::Matcha;
	uiThemeDefaults[7]  = &Theme::Themes::Lavender;
	uiThemeDefaults[8]  = &Theme::Themes::Snow;
	uiThemeDefaults[9]  = &Theme::Themes::Linen;
	uiThemeDefaults[10] = &Theme::Themes::Frost;
	uiThemeDefaults[11] = &Theme::Themes::Blossom;
	for (int i = 0; i < uiThemeCount; i++) {
		uiThemes[i] = *uiThemeDefaults[i]; // copy
	}
	currentTheme = 0;
	editingTheme = -1;
	editingSlot = -1;
	slotPicker.Layout(0, 0, 180);
	slotHexInput.Layout(0, 0, 100, L"#000000");

	// === Hex color input ===
	hexColorInput.Layout(cx, 0, 100, L"#7F9BD4");
	{
		wchar_t hexBuf[16];
		swprintf_s(hexBuf, L"#%02X%02X%02X",
			(int)(Theme::Custom::AccentR * 255),
			(int)(Theme::Custom::AccentG * 255),
			(int)(Theme::Custom::AccentB * 255));
		wcscpy_s(hexColorInput.buffer, hexBuf);
		hexColorInput.cursor = (int)wcslen(hexBuf);
	}

	// === Profile system ===
	const wchar_t* profileNames[] = { L"Default", L"Drawing", L"Aim", L"Custom" };
	for (int i = 0; i < MAX_PROFILES; i++) {
		profiles[i].name = profileNames[i];
		profiles[i].exists = (i == 0);
		profileBtns[i].Layout(0, 0, 80, 26, profileNames[i], false);
	}

	// === Input Visualizer ===
	visualizerToggle.Layout(cx, 0, L"Input Visualizer", L"Show pen trail overlay on tablet area");

}

void AetherApp::OnMouseMove(float x, float y) {
	// Drag-scroll: update scroll offset based on mouse Y delta
	if (isDragScrolling) {
		float dy = dragScrollStartY - y; // invert: drag up = scroll down
		float* scrollTarget = nullptr;
		switch (sidebar.activeIndex) {
		case 0: scrollTarget = &areaScrollY; break;
		case 1: scrollTarget = &filterScrollY; break;
		case 2: scrollTarget = &settingsScrollY; break;
		}
		if (scrollTarget) {
			*scrollTarget = dragScrollStartOffset + dy;
			if (*scrollTarget < 0) *scrollTarget = 0;
			ClampScrollOffsets();
		}
	}
	mouseX = x; mouseY = y;
}
void AetherApp::OnMouseDown() {
	mouseDown = true;
	mouseClicked = true;
}
void AetherApp::OnMouseUp() {
	mouseDown = false;
	if (isDraggingArea) {
		isDraggingArea = false;
		dragTarget = 0;
		ApplyAllSettings();
	}
}

void AetherApp::OnMiddleMouseDown() {
	middleMouseDown = true;
	// Start drag-scroll if cursor is in content area
	if (mouseX > Theme::Size::SidebarWidth && mouseY > GetContentAreaTop() && mouseY < GetContentAreaBottom()) {
		isDragScrolling = true;
		dragScrollStartY = mouseY;
		switch (sidebar.activeIndex) {
		case 0: dragScrollStartOffset = areaScrollY; break;
		case 1: dragScrollStartOffset = filterScrollY; break;
		case 2: dragScrollStartOffset = settingsScrollY; break;
		default: dragScrollStartOffset = 0; break;
		}
	}
}

void AetherApp::OnMiddleMouseUp() {
	middleMouseDown = false;
	isDragScrolling = false;
}

void AetherApp::OnRightMouseDown() {
	rightMouseDown = true;
	// Also start drag-scroll with RMB
	if (mouseX > Theme::Size::SidebarWidth && mouseY > GetContentAreaTop() && mouseY < GetContentAreaBottom()) {
		isDragScrolling = true;
		dragScrollStartY = mouseY;
		switch (sidebar.activeIndex) {
		case 0: dragScrollStartOffset = areaScrollY; break;
		case 1: dragScrollStartOffset = filterScrollY; break;
		case 2: dragScrollStartOffset = settingsScrollY; break;
		default: dragScrollStartOffset = 0; break;
		}
	}
}

void AetherApp::OnRightMouseUp() {
	rightMouseDown = false;
	isDragScrolling = false;
}
void AetherApp::RefreshDetectedScreen() {
	int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
	int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
	int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	virtualScreenX = (float)left;
	virtualScreenY = (float)top;
	detectedScreenW = (float)((width > 0) ? width : GetSystemMetrics(SM_CXSCREEN));
	detectedScreenH = (float)((height > 0) ? height : GetSystemMetrics(SM_CYSCREEN));

	displayTargets.clear();
	DisplayTarget desktop;
	desktop.x = 0;
	desktop.y = 0;
	desktop.width = detectedScreenW;
	desktop.height = detectedScreenH;
	desktop.label = L"Full desktop";
	displayTargets.push_back(desktop);

	MonitorEnumContext ctx;
	ctx.targets = &displayTargets;
	ctx.virtualLeft = left;
	ctx.virtualTop = top;
	EnumDisplayMonitors(nullptr, nullptr, EnumDisplayTargetProc, reinterpret_cast<LPARAM>(&ctx));

	if (selectedDisplayTarget >= (int)displayTargets.size())
		selectedDisplayTarget = 0;
}

void AetherApp::SendDisplaySettingsToDriver() {
	if (!driver.isConnected)
		return;

	char cmd[256];
	sprintf_s(cmd, "DesktopSize %.0f %.0f", detectedScreenW, detectedScreenH);
	driver.SendCommand(cmd);

	// Convert GUI-relative screen coordinates to actual virtual desktop coordinates
	// GUI stores offsets relative to virtual screen origin, but the driver/VMulti
	// needs actual pixel coordinates in the virtual desktop coordinate space
	float actualScreenX = area.screenX.value + virtualScreenX;
	float actualScreenY = area.screenY.value + virtualScreenY;
	sprintf_s(cmd, "ScreenArea %.0f %.0f %.0f %.0f",
		area.screenWidth.value, area.screenHeight.value,
		actualScreenX, actualScreenY);
	driver.SendCommand(cmd);
	driver.SendCommand("UpdateMonitorInfo");
}

void AetherApp::SendStartupSettingsToDriver() {
	if (!driver.isConnected)
		return;

	char cmd[256];
	sprintf_s(cmd, "DesktopSize %.0f %.0f", detectedScreenW, detectedScreenH);
	driver.SendCommand(cmd);

	// Convert GUI-relative screen coordinates to actual virtual desktop coordinates
	float actualScreenX = area.screenX.value + virtualScreenX;
	float actualScreenY = area.screenY.value + virtualScreenY;
	sprintf_s(cmd, "ScreenArea %.0f %.0f %.0f %.0f",
		area.screenWidth.value, area.screenHeight.value,
		actualScreenX, actualScreenY);
	driver.SendCommand(cmd);
	sprintf_s(cmd, "TabletArea %.2f %.2f %.2f %.2f",
		area.tabletWidth.value, area.tabletHeight.value,
		area.tabletX.value, area.tabletY.value);
	driver.SendCommand(cmd);

	sprintf_s(cmd, "Rotate %.1f", area.rotation.value);
	driver.SendCommand(cmd);

	const char* modes[] = { "Mode Absolute", "Mode Relative", "Mode Digitizer" };
	driver.SendCommand(modes[outputMode.selected]);

	sprintf_s(cmd, "Sensitivity %.4f", area.screenWidth.value / area.tabletWidth.value);
	driver.SendCommand(cmd);

	driver.SendCommand("UpdateMonitorInfo");

	sprintf_s(cmd, "ButtonMap %d %d %d",
		buttonTip.selected, buttonBottom.selected, buttonTop.selected);
	driver.SendCommand(cmd);

	driver.SendCommand(areaClipping.value ? "AreaClipping on" : "AreaClipping off");
	driver.SendCommand(areaLimiting.value ? "AreaLimiting on" : "AreaLimiting off");
	sprintf_s(cmd, "TipThreshold %.0f", tipThreshold.value);
	driver.SendCommand(cmd);
	sprintf_s(cmd, "Overclock %s %.0f", overclockEnabled.value ? "on" : "off", overclockHz.value);
	driver.SendCommand(cmd);

	SendFilterSettings();

	driver.SendCommand("start");
}

bool AetherApp::StartDriverService() {
	if (driver.isConnected)
		return true;

	ClampScreenArea();
	ApplyAspectLock(false);
	if (!driver.Start(servicePath))
		return false;

	Sleep(800);
	SendStartupSettingsToDriver();
	return true;
}

void AetherApp::ApplyDisplayTarget(int index) {
	if (displayTargets.empty())
		return;

	if (index < 0)
		index = (int)displayTargets.size() - 1;
	if (index >= (int)displayTargets.size())
		index = 0;

	selectedDisplayTarget = index;
	const DisplayTarget& target = displayTargets[selectedDisplayTarget];
	area.screenWidth.value = target.width;
	area.screenHeight.value = target.height;
	area.screenX.value = target.x;
	area.screenY.value = target.y;

	ClampScreenArea();
	ApplyAspectLock(false);
	SendStartupSettingsToDriver();
}

float AetherApp::GetScreenAspectRatio() const {
	float screenW = area.screenWidth.value;
	float screenH = area.screenHeight.value;

	if (screenW <= 1.0f) screenW = (detectedScreenW > 1.0f) ? detectedScreenW : 1920.0f;
	if (screenH <= 1.0f) screenH = (detectedScreenH > 1.0f) ? detectedScreenH : 1080.0f;

	return Clamp(screenW / screenH, 0.1f, 10.0f);
}

void AetherApp::ClampScreenArea() {
	float maxW = std::max(100.0f, detectedScreenW);
	float maxH = std::max(100.0f, detectedScreenH);

	area.screenWidth.maxVal = maxW;
	area.screenHeight.maxVal = maxH;
	area.screenX.maxVal = maxW;
	area.screenY.maxVal = maxH;

	area.screenWidth.value = Clamp(area.screenWidth.value, area.screenWidth.minVal, maxW);
	area.screenHeight.value = Clamp(area.screenHeight.value, area.screenHeight.minVal, maxH);

	float maxX = std::max(0.0f, detectedScreenW - area.screenWidth.value);
	float maxY = std::max(0.0f, detectedScreenH - area.screenHeight.value);
	area.screenX.value = Clamp(area.screenX.value, 0.0f, maxX);
	area.screenY.value = Clamp(area.screenY.value, 0.0f, maxY);
}

void AetherApp::ClampTabletAreaToFull(float fullTabletW, float fullTabletH) {
	area.tabletWidth.value = Clamp(area.tabletWidth.value, area.tabletWidth.minVal, fullTabletW);
	area.tabletHeight.value = Clamp(area.tabletHeight.value, area.tabletHeight.minVal, fullTabletH);

	float halfW = area.tabletWidth.value * 0.5f;
	float halfH = area.tabletHeight.value * 0.5f;
	area.tabletX.value = Clamp(area.tabletX.value, halfW, fullTabletW - halfW);
	area.tabletY.value = Clamp(area.tabletY.value, halfH, fullTabletH - halfH);
}

void AetherApp::ApplyAspectLock(bool preserveHeight) {
	float fullTabletW = (driver.tabletWidth > 1.0f) ? driver.tabletWidth : 152.0f;
	float fullTabletH = (driver.tabletHeight > 1.0f) ? driver.tabletHeight : 95.0f;

	if (area.lockAspect.value) {
		float aspect = GetScreenAspectRatio();

		if (preserveHeight) {
			area.tabletWidth.value = area.tabletHeight.value * aspect;
			if (area.tabletWidth.value > fullTabletW) {
				area.tabletWidth.value = fullTabletW;
				area.tabletHeight.value = area.tabletWidth.value / aspect;
			}
		}
		else {
			area.tabletHeight.value = area.tabletWidth.value / aspect;
			if (area.tabletHeight.value > fullTabletH) {
				area.tabletHeight.value = fullTabletH;
				area.tabletWidth.value = area.tabletHeight.value * aspect;
			}
		}
	}

	ClampTabletAreaToFull(fullTabletW, fullTabletH);
}

void AetherApp::OnResize(UINT width, UINT height) {
	if (width == 0 || height == 0)
		return;

	clientWidth = (float)width;
	clientHeight = (float)height;
	Theme::Runtime::SetWindowSize(clientWidth, clientHeight);
	renderer.Resize(width, height);
	ClampScrollOffsets();
}

void AetherApp::OnDisplayChange() {
	float oldW = detectedScreenW;
	float oldH = detectedScreenH;
	RefreshDetectedScreen();

	if (selectedDisplayTarget > 0 && selectedDisplayTarget < (int)displayTargets.size()) {
		ApplyDisplayTarget(selectedDisplayTarget);
		return;
	}

	if (fabsf(area.screenWidth.value - oldW) < 1.0f) area.screenWidth.value = detectedScreenW;
	if (fabsf(area.screenHeight.value - oldH) < 1.0f) area.screenHeight.value = detectedScreenH;
	ClampScreenArea();
	ApplyAspectLock(false);

	SendDisplaySettingsToDriver();
}
void AetherApp::OnMouseWheel(float delta) {
	scrollDelta += delta;
	float scrollSpeed = 40.0f;
	switch (sidebar.activeIndex) {
	case 0: areaScrollY -= delta * scrollSpeed; break;
	case 1: filterScrollY -= delta * scrollSpeed; break;
	case 2: settingsScrollY -= delta * scrollSpeed; break;
	}
	if (areaScrollY < 0) areaScrollY = 0;
	if (filterScrollY < 0) filterScrollY = 0;
	ClampScrollOffsets();
}

void AetherApp::OnChar(wchar_t ch) {
	area.tabletWidth.OnChar(ch); area.tabletHeight.OnChar(ch);
	area.tabletX.OnChar(ch); area.tabletY.OnChar(ch);
	area.screenWidth.OnChar(ch); area.screenHeight.OnChar(ch);
	area.screenX.OnChar(ch); area.screenY.OnChar(ch);
	area.rotation.OnChar(ch);
	tipThreshold.OnChar(ch);
	overclockHz.OnChar(ch);
	filters.smoothingLatency.OnChar(ch); filters.smoothingInterval.OnChar(ch);
	filters.antichatterStrength.OnChar(ch); filters.antichatterMultiplier.OnChar(ch);
	filters.antichatterOffsetX.OnChar(ch); filters.antichatterOffsetY.OnChar(ch);
	filters.noiseBuffer.OnChar(ch); filters.noiseThreshold.OnChar(ch); filters.noiseIterations.OnChar(ch);
	filters.snapRadius.OnChar(ch); filters.snapSmooth.OnChar(ch);
	filters.reconStrength.OnChar(ch); filters.reconVelSmooth.OnChar(ch);
	filters.reconAccelCap.OnChar(ch); filters.reconPredTime.OnChar(ch);
	filters.adaptiveProcessNoise.OnChar(ch); filters.adaptiveMeasNoise.OnChar(ch); filters.adaptiveVelWeight.OnChar(ch);
	aether.lagRemovalStrength.OnChar(ch); aether.stabilizerStability.OnChar(ch); aether.stabilizerSensitivity.OnChar(ch);
	aether.snappingInner.OnChar(ch); aether.snappingOuter.OnChar(ch); aether.suppressionTime.OnChar(ch);

	// Slot hex input in theme editor — apply on Enter
	if (slotHexInput.OnChar(ch)) {
		if (editingTheme >= 0 && editingTheme < uiThemeCount && editingSlot >= 0) {
			std::wstring hex = slotHexInput.GetText();
			const wchar_t* p = hex.c_str();
			if (p[0] == L'#') p++;
			if (wcslen(p) >= 6) {
				int ri = 0, gi = 0, bi = 0;
				swscanf_s(p, L"%02x%02x%02x", &ri, &gi, &bi);
				float r = ri / 255.0f, g = gi / 255.0f, b = bi / 255.0f;
				SetThemeSlotColor(uiThemes[editingTheme], editingSlot, r, g, b);
				slotPicker.SetRGB(r, g, b);
				if (editingTheme == currentTheme) {
					Theme::ApplyTheme(uiThemes[editingTheme]);
					if (hWnd) { extern void ApplyAetherWindowTheme(HWND); ApplyAetherWindowTheme(hWnd); }
				}
				AutoSaveConfig();
			}
		}
	}

	// Hex color input — apply on Enter
	if (hexColorInput.OnChar(ch)) {
		std::wstring hex = hexColorInput.GetText();
		const wchar_t* p = hex.c_str();
		if (p[0] == L'#') p++;
		if (wcslen(p) >= 6) {
			int ri = 0, gi = 0, bi = 0;
			swscanf_s(p, L"%02x%02x%02x", &ri, &gi, &bi);
			Theme::Custom::SetAccent(ri / 255.0f, gi / 255.0f, bi / 255.0f);
			accentPicker.SetRGB(ri / 255.0f, gi / 255.0f, bi / 255.0f);
			AutoSaveConfig();
		}
	}

	// Console input
	if (consoleInput.OnChar(ch)) {
		std::wstring wcmd = consoleInput.GetText();
		if (wcmd.length() > 0) {
			std::string cmd = WideToUtf8(wcmd);
			driver.SendCommand(cmd);
			commandHistory.push_back(cmd);
			commandHistoryIdx = (int)commandHistory.size();
			consoleInput.Clear();
		}
	}
}

void AetherApp::OnKeyDown(int vk) {
	bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	Slider* editableSliders[] = {
		&area.tabletWidth, &area.tabletHeight, &area.tabletX, &area.tabletY,
		&area.screenWidth, &area.screenHeight, &area.screenX, &area.screenY,
		&area.rotation, &tipThreshold, &overclockHz,
		&filters.smoothingLatency, &filters.smoothingInterval,
		&filters.antichatterStrength, &filters.antichatterMultiplier,
		&filters.antichatterOffsetX, &filters.antichatterOffsetY,
		&filters.noiseBuffer, &filters.noiseThreshold, &filters.noiseIterations,
		&filters.snapRadius, &filters.snapSmooth,
		&filters.reconStrength, &filters.reconVelSmooth,
		&filters.reconAccelCap, &filters.reconPredTime,
		&filters.adaptiveProcessNoise, &filters.adaptiveMeasNoise, &filters.adaptiveVelWeight,
		&aether.lagRemovalStrength, &aether.stabilizerStability, &aether.stabilizerSensitivity,
		&aether.snappingInner, &aether.snappingOuter, &aether.suppressionTime
	};
	for (Slider* slider : editableSliders) {
		if (slider->OnKeyDown(vk, ctrl, shift)) return;
	}

	// TextInput key handling (selection, clipboard, arrows)
	if (slotHexInput.OnKeyDown(vk)) return;
	if (hexColorInput.OnKeyDown(vk)) return;
	if (consoleInput.OnKeyDown(vk)) return;

	if (consoleInput.focused && !commandHistory.empty()) {
		if (vk == VK_UP) {
			commandHistoryIdx--;
			if (commandHistoryIdx < 0) commandHistoryIdx = 0;
			std::wstring wcmd(commandHistory[commandHistoryIdx].begin(), commandHistory[commandHistoryIdx].end());
			wcscpy_s(consoleInput.buffer, wcmd.c_str());
			consoleInput.cursor = (int)wcmd.length();
		}
		if (vk == VK_DOWN) {
			commandHistoryIdx++;
			if (commandHistoryIdx >= (int)commandHistory.size()) {
				commandHistoryIdx = (int)commandHistory.size();
				consoleInput.Clear();
			} else {
				std::wstring wcmd(commandHistory[commandHistoryIdx].begin(), commandHistory[commandHistoryIdx].end());
				wcscpy_s(consoleInput.buffer, wcmd.c_str());
				consoleInput.cursor = (int)wcmd.length();
			}
		}
	}
}

float AetherApp::GetContentAreaTop() {
	return Theme::Size::HeaderHeight;
}

float AetherApp::GetContentAreaBottom() {
	return Theme::Runtime::WindowHeight - 28;
}

void AetherApp::BeginClipContent() {
	if (!renderer.pRT) return;
	D2D1_RECT_F clip = D2D1::RectF(
		Theme::Size::SidebarWidth, GetContentAreaTop(),
		Theme::Runtime::WindowWidth, GetContentAreaBottom()
	);
	renderer.pRT->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
}

void AetherApp::EndClipContent() {
	if (!renderer.pRT) return;
	renderer.pRT->PopAxisAlignedClip();
}

void AetherApp::Tick() {
	auto now = std::chrono::high_resolution_clock::now();
	deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
	if (deltaTime > 0.1f) deltaTime = 0.016f;
	lastFrameTime = now;

	Tooltip::Reset();

	// Age trail points
	{
		std::lock_guard<std::mutex> lock(driver.trailMutex);
		for (auto& p : driver.trail) p.age += deltaTime;
		// Remove old points (>3 seconds)
		while (!driver.trail.empty() && driver.trail.front().age > 3.0f)
			driver.trail.erase(driver.trail.begin());
	}

	renderer.BeginFrame();
	DrawBackground();
	DrawHeader();

	int oldTab = sidebar.activeIndex;
	sidebar.Update(mouseX, mouseY, mouseClicked, deltaTime);
	sidebar.Draw(renderer);

	if (sidebar.activeIndex != oldTab && oldTab >= 0) {
		prevTab = oldTab;
		tabTransitionT = 0.0f;
		tabSlideOffset = (sidebar.activeIndex > oldTab) ? 40.0f : -40.0f;
	}
	if (tabTransitionT < 1.0f) {
		tabTransitionT += deltaTime * Theme::Anim::SpeedFast * 0.7f;
		if (tabTransitionT > 1.0f) tabTransitionT = 1.0f;
	}
	float easeT = 1.0f - (1.0f - tabTransitionT) * (1.0f - tabTransitionT);
	tabFadeAlpha = easeT;
	float currentSlide = tabSlideOffset * (1.0f - easeT);

	BeginClipContent();
	D2D1_MATRIX_3X2_F oldTransform;
	renderer.pRT->GetTransform(&oldTransform);
	renderer.pRT->SetTransform(
		D2D1::Matrix3x2F::Translation(0, currentSlide) * oldTransform);

	switch (sidebar.activeIndex) {
	case 0: DrawAreaPanel(); break;
	case 1: DrawFilterPanel(); break;
	case 2: DrawSettingsPanel(); break;
	case 3: DrawConsolePanel(); break;
	case 4: DrawAboutPanel(); break;
	}

	renderer.pRT->SetTransform(oldTransform);
	EndClipContent();

	// === Scrollbar indicator + interaction ===
	{
		float contentH = 0, scrollY = 0;
		float* scrollPtr = nullptr;
		switch (sidebar.activeIndex) {
		case 0: contentH = areaContentH; scrollY = areaScrollY; scrollPtr = &areaScrollY; break;
		case 1: contentH = filterContentH; scrollY = filterScrollY; scrollPtr = &filterScrollY; break;
		case 2: contentH = settingsContentH; scrollY = settingsScrollY; scrollPtr = &settingsScrollY; break;
		}
		float visibleH = GetContentAreaBottom() - GetContentAreaTop();
		if (contentH > visibleH + 1.0f && visibleH > 10.0f) {
			float trackX = Theme::Runtime::WindowWidth - 6.0f;
			float trackY = GetContentAreaTop() + 2.0f;
			float trackH = visibleH - 4.0f;
			float thumbRatio = visibleH / contentH;
			float thumbH = trackH * thumbRatio;
			if (thumbH < 20.0f) thumbH = 20.0f;
			float maxScroll = contentH - visibleH;
			float scrollRatio = (maxScroll > 0) ? (scrollY / maxScroll) : 0;
			float thumbY = trackY + (trackH - thumbH) * scrollRatio;
			// Track
			D2D1_COLOR_F trackCol = Theme::BorderSubtle();
			trackCol.a = 0.15f;
			renderer.FillRoundedRect(trackX, trackY, 4.0f, trackH, 2.0f, trackCol);
			// Click on scrollbar track to jump
			if (scrollPtr && mouseClicked && PointInRect(mouseX, mouseY, trackX - 4.0f, trackY, 12.0f, trackH)) {
				float clickRatio = (mouseY - trackY) / trackH;
				*scrollPtr = maxScroll * clickRatio;
				ClampScrollOffsets();
			}
			// Thumb — highlight on hover
			bool thumbHovered = PointInRect(mouseX, mouseY, trackX - 4.0f, thumbY, 12.0f, thumbH);
			D2D1_COLOR_F thumbCol = (isDragScrolling || thumbHovered) ? Theme::AccentPrimary() : Theme::TextMuted();
			thumbCol.a = (isDragScrolling || thumbHovered) ? 0.6f : 0.3f;
			renderer.FillRoundedRect(trackX, thumbY, 4.0f, thumbH, 2.0f, thumbCol);
		}
	}

	DrawStatusBar();
	Tooltip::Draw(renderer);
	renderer.EndFrame();

	mouseClicked = false;
	scrollDelta = 0;
}

void AetherApp::UpdateControls() {}

void AetherApp::SendFilterSettings() {
	if (!driver.isConnected)
		return;

	char cmd[256];

	if (filters.smoothingEnabled.value) {
		sprintf_s(cmd, "SmoothingInterval %d", (int)filters.smoothingInterval.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "Smoothing %.2f", filters.smoothingLatency.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("Smoothing off");
	}

	sprintf_s(cmd, "AntichatterEnabled %d", filters.antichatterEnabled.value ? 1 : 0);
	driver.SendCommand(cmd);
	if (filters.antichatterEnabled.value) {
		sprintf_s(cmd, "AntichatterStrength %.2f", filters.antichatterStrength.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AntichatterMultiplier %.2f", filters.antichatterMultiplier.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AntichatterOffsetX %.4f", filters.antichatterOffsetX.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AntichatterOffsetY %.4f", filters.antichatterOffsetY.value);
		driver.SendCommand(cmd);
	}

	if (filters.noiseEnabled.value) {
		sprintf_s(cmd, "Noise %.0f %.4f %.0f",
			filters.noiseBuffer.value, filters.noiseThreshold.value,
			filters.noiseIterations.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("Noise off");
	}

	if (filters.velCurveEnabled.value) {
		sprintf_s(cmd, "PredictionEnabled 1");
		driver.SendCommand(cmd);
		sprintf_s(cmd, "PredictionStrength %.4f", filters.velCurveSmoothing.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "PredictionSharpness %.4f", filters.velCurveSharpness.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "PredictionOffsetX %.4f", filters.velCurveMinSpeed.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "PredictionOffsetY %.4f", filters.velCurveMaxSpeed.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("PredictionEnabled 0");
	}

	if (filters.reconstructorEnabled.value) {
		sprintf_s(cmd, "Reconstructor %.4f %.4f %.2f %.2f",
			filters.reconStrength.value, filters.reconVelSmooth.value,
			filters.reconAccelCap.value, filters.reconPredTime.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("Reconstructor off");
	}

	if (filters.adaptiveEnabled.value) {
		sprintf_s(cmd, "Adaptive %.6f %.6f %.4f",
			filters.adaptiveProcessNoise.value, filters.adaptiveMeasNoise.value,
			filters.adaptiveVelWeight.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("Adaptive off");
	}

	// Aether Smooth
	if (aether.enabled.value) {
		driver.SendCommand("AetherSmooth on");
		sprintf_s(cmd, "AS_LagRemoval %d %.4f",
			aether.lagRemovalEnabled.value ? 1 : 0, aether.lagRemovalStrength.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AS_Stabilizer %d %.4f %.6f",
			aether.stabilizerEnabled.value ? 1 : 0,
			aether.stabilizerStability.value, aether.stabilizerSensitivity.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AS_Snapping %d %.4f %.4f",
			aether.snappingEnabled.value ? 1 : 0,
			aether.snappingInner.value, aether.snappingOuter.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AS_Suppress %d %.4f",
			aether.suppressionEnabled.value ? 1 : 0, aether.suppressionTime.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("AetherSmooth off");
	}
}

void AetherApp::ApplyAllSettings() {
	if (!driver.isConnected)
		return;

	char cmd[256];
	ClampScreenArea();

	sprintf_s(cmd, "DesktopSize %.0f %.0f", detectedScreenW, detectedScreenH);
	driver.SendCommand(cmd);

	// Convert GUI-relative screen coordinates to actual virtual desktop coordinates
	{
		float actualScreenX = area.screenX.value + virtualScreenX;
		float actualScreenY = area.screenY.value + virtualScreenY;
		sprintf_s(cmd, "ScreenArea %.0f %.0f %.0f %.0f",
			area.screenWidth.value, area.screenHeight.value,
			actualScreenX, actualScreenY);
		driver.SendCommand(cmd);
	}

	sprintf_s(cmd, "TabletArea %.2f %.2f %.2f %.2f",
		area.tabletWidth.value, area.tabletHeight.value,
		area.tabletX.value, area.tabletY.value);
	driver.SendCommand(cmd);

	sprintf_s(cmd, "Rotate %.1f", area.rotation.value);
	driver.SendCommand(cmd);

	const char* modes[] = { "Mode Absolute", "Mode Relative", "Mode Digitizer" };
	driver.SendCommand(modes[outputMode.selected]);

	sprintf_s(cmd, "Sensitivity %.4f", area.screenWidth.value / area.tabletWidth.value);
	driver.SendCommand(cmd);

	driver.SendCommand("UpdateMonitorInfo");

	sprintf_s(cmd, "ButtonMap %d %d %d",
		buttonTip.selected, buttonBottom.selected, buttonTop.selected);
	driver.SendCommand(cmd);

	driver.SendCommand(areaClipping.value ? "AreaClipping on" : "AreaClipping off");
	driver.SendCommand(areaLimiting.value ? "AreaLimiting on" : "AreaLimiting off");

	sprintf_s(cmd, "TipThreshold %.0f", tipThreshold.value);
	driver.SendCommand(cmd);

	sprintf_s(cmd, "Overclock %s %.0f", overclockEnabled.value ? "on" : "off", overclockHz.value);
	driver.SendCommand(cmd);

	SendFilterSettings();

	AutoSaveConfig();
}

std::wstring AetherApp::GetConfigPath() {
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring dir(exePath);
	size_t slash = dir.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
		dir = dir.substr(0, slash + 1);
	return dir + L"aether_config.cfg";
}

void AetherApp::AutoSaveConfig() {
	SaveConfig(GetConfigPath());
}

void AetherApp::AutoLoadConfig() {
	std::wstring path = GetConfigPath();
	DWORD attrs = GetFileAttributesW(path.c_str());
	if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
		LoadConfig(path);
	}
}

void AetherApp::ClampScrollOffsets() {
	if (areaScrollY < 0) areaScrollY = 0;
	if (filterScrollY < 0) filterScrollY = 0;
	if (settingsScrollY < 0) settingsScrollY = 0;

	float visibleH = GetContentAreaBottom() - GetContentAreaTop();
	float maxScroll = areaContentH - visibleH;
	if (maxScroll < 0) maxScroll = 0;
	if (areaScrollY > maxScroll) areaScrollY = maxScroll;

	maxScroll = filterContentH - visibleH;
	if (maxScroll < 0) maxScroll = 0;
	if (filterScrollY > maxScroll) filterScrollY = maxScroll;

	maxScroll = settingsContentH - visibleH;
	if (maxScroll < 0) maxScroll = 0;
	if (settingsScrollY > maxScroll) settingsScrollY = maxScroll;
}

void AetherApp::DrawBackground() {
	float w = Theme::Runtime::WindowWidth;
	float h = Theme::Runtime::WindowHeight;

	// Deep BG is the Clear() color from BeginFrame (fills entire window).
	// BgBase fills the content area — uses Deep BG color blended slightly with BgBase
	// so that changing Deep BG in theme editor has a visible effect on the main background.
	D2D1_COLOR_F contentBg = LerpColor(Theme::BgDeep(), Theme::BgBase(), 0.85f);
	renderer.FillRect(0, 0, w, h, contentBg);

	bgAnimT += deltaTime;

	// === Dot grid (always drawn, subtle) ===
	D2D1_COLOR_F dot = Theme::BorderNormal();
	dot.a = 0.04f;
	float startX = Theme::Size::SidebarWidth + 20.0f;
	float startY = Theme::Size::HeaderHeight + 18.0f;
	for (float gy = startY; gy < h - 36.0f; gy += 28.0f) {
		for (float gx = startX; gx < w - 12.0f; gx += 28.0f) {
			renderer.FillCircle(gx, gy, 0.6f, dot);
		}
	}

	if (particleStyle == 3) return;

	if (particleStyle == 0) {
		starSpawnTimer -= deltaTime;
		if (starSpawnTimer <= 0.0f) {
			for (int i = 0; i < MAX_STARS; i++) {
				if (!stars[i].active) {
					ShootingStar& s = stars[i];
					s.active = true;
					bool fromTop = (rand() % 3) != 0;
					if (fromTop) {
						s.x = w * 0.1f + (rand() % (int)(w * 0.9f));
						s.y = -10.0f;
					} else {
						s.x = w + 10.0f;
						s.y = (float)(rand() % (int)(h * 0.4f));
					}
					float angle = 2.7f + (rand() % 80) / 100.0f;
					float speed = 100.0f + (float)(rand() % 220);
					s.vx = cosf(angle) * speed;
					s.vy = sinf(angle) * speed;
					s.maxLife = 1.8f + (rand() % 250) / 100.0f;
					s.life = s.maxLife;
					s.tailLen = 35.0f + (rand() % 60);
					s.brightness = 0.05f + (rand() % 10) / 100.0f;
					break;
				}
			}
			starSpawnTimer = 2.0f + (rand() % 400) / 100.0f;
		}

		for (int i = 0; i < MAX_STARS; i++) {
			ShootingStar& s = stars[i];
			if (!s.active) continue;

			s.x += s.vx * deltaTime;
			s.y += s.vy * deltaTime;
			s.life -= deltaTime;

			if (s.life <= 0 || s.x < -100 || s.x > w + 100 || s.y > h + 100) {
				s.active = false;
				continue;
			}

			float lifeT = s.life / s.maxLife;
			float fade = (lifeT > 0.85f) ? (1.0f - lifeT) / 0.15f : (lifeT < 0.25f) ? lifeT / 0.25f : 1.0f;
			float alpha = s.brightness * fade;

			float speed = sqrtf(s.vx * s.vx + s.vy * s.vy);
			float nx = (speed > 0.1f) ? s.vx / speed : 0;
			float ny = (speed > 0.1f) ? s.vy / speed : 0;

			// Glow around head
			D2D1_COLOR_F glowCol = Theme::AccentPrimary();
			glowCol.a = alpha * 0.3f;
			renderer.FillCircle(s.x, s.y, 4.0f, glowCol);

			// Tail segments with gradient
			int segments = 8;
			for (int seg = 0; seg < segments; seg++) {
				float t = (float)seg / (float)segments;
				float tx = s.x - nx * s.tailLen * t;
				float ty = s.y - ny * s.tailLen * t;
				float segAlpha = alpha * (1.0f - t * 0.9f);
				float segR = 1.4f * (1.0f - t * 0.7f);
				D2D1_COLOR_F starCol = LerpColor(Theme::AccentPrimary(), Theme::AccentSecondary(), t);
				starCol.a = segAlpha;
				renderer.FillCircle(tx, ty, segR, starCol);
			}

			// Bright head
			D2D1_COLOR_F headCol = D2D1::ColorF(0xFFFFFF, alpha * 1.8f);
			renderer.FillCircle(s.x, s.y, 1.6f, headCol);
		}
	}

	// ========================================
	// Style 1: Fireflies
	// ========================================
	else if (particleStyle == 1) {
		if (!firefliesInitialized) {
			for (int i = 0; i < MAX_FIREFLIES; i++) {
				Firefly& f = fireflies[i];
				f.active = true;
				f.baseX = Theme::Size::SidebarWidth + 30.0f + (float)(rand() % (int)(w - Theme::Size::SidebarWidth - 60));
				f.baseY = Theme::Size::HeaderHeight + 20.0f + (float)(rand() % (int)(h - Theme::Size::HeaderHeight - 80));
				f.x = f.baseX;
				f.y = f.baseY;
				f.phase = (rand() % 1000) / 100.0f;
				f.speed = 0.3f + (rand() % 100) / 100.0f;
				f.radius = 1.5f + (rand() % 20) / 10.0f;
				f.alpha = 0.0f;
				f.wanderAngle = (rand() % 628) / 100.0f;
			}
			firefliesInitialized = true;
		}

		for (int i = 0; i < MAX_FIREFLIES; i++) {
			Firefly& f = fireflies[i];
			if (!f.active) continue;

			f.phase += deltaTime * f.speed;
			f.wanderAngle += (((rand() % 100) - 50) / 500.0f) * deltaTime * 60.0f;

			// Gentle wandering motion
			f.x += cosf(f.wanderAngle) * 12.0f * deltaTime;
			f.y += sinf(f.wanderAngle) * 8.0f * deltaTime;

			// Soft boundary pull back to base
			float dx = f.baseX - f.x;
			float dy = f.baseY - f.y;
			f.x += dx * 0.3f * deltaTime;
			f.y += dy * 0.3f * deltaTime;

			// Pulsing glow
			float pulse = (sinf(f.phase * 1.5f) * 0.5f + 0.5f);
			float targetAlpha = pulse * 0.12f + 0.02f;
			f.alpha = Lerp(f.alpha, targetAlpha, deltaTime * 2.0f);

			// Outer glow
			D2D1_COLOR_F glowOuter = Theme::AccentPrimary();
			glowOuter.a = f.alpha * 0.3f;
			renderer.FillCircle(f.x, f.y, f.radius * 3.0f, glowOuter);

			// Inner glow
			D2D1_COLOR_F glowInner = Theme::AccentSecondary();
			glowInner.a = f.alpha * 0.7f;
			renderer.FillCircle(f.x, f.y, f.radius * 1.5f, glowInner);

			// Core
			D2D1_COLOR_F core = D2D1::ColorF(0xFFFFFF, f.alpha * 1.2f);
			renderer.FillCircle(f.x, f.y, f.radius * 0.5f, core);
		}
	}

	else if (particleStyle == 2) {
		if (!snowInitialized) {
			for (int i = 0; i < MAX_SNOWFLAKES; i++) {
				Snowflake& s = snowflakes[i];
				s.active = true;
				s.x = Theme::Size::SidebarWidth + (float)(rand() % (int)(w - Theme::Size::SidebarWidth));
				s.y = (float)(rand() % (int)h);
				s.speed = 8.0f + (rand() % 25);
				s.drift = ((rand() % 100) - 50) / 50.0f * 4.0f;
				s.size = 0.8f + (rand() % 20) / 10.0f;
				s.alpha = 0.03f + (rand() % 8) / 100.0f;
				s.wobblePhase = (rand() % 628) / 100.0f;
			}
			snowInitialized = true;
		}

		for (int i = 0; i < MAX_SNOWFLAKES; i++) {
			Snowflake& s = snowflakes[i];
			if (!s.active) continue;

			s.wobblePhase += deltaTime * 1.5f;
			s.y += s.speed * deltaTime;
			s.x += (s.drift + sinf(s.wobblePhase) * 6.0f) * deltaTime;

			// Wrap around
			if (s.y > h + 10) {
				s.y = -5.0f;
				s.x = Theme::Size::SidebarWidth + (float)(rand() % (int)(w - Theme::Size::SidebarWidth));
			}
			if (s.x < Theme::Size::SidebarWidth - 5) s.x = w;
			if (s.x > w + 5) s.x = Theme::Size::SidebarWidth;

			// Soft glow
			D2D1_COLOR_F glow = Theme::AccentSecondary();
			glow.a = s.alpha * 0.4f;
			renderer.FillCircle(s.x, s.y, s.size * 2.5f, glow);

			// Core
			D2D1_COLOR_F core = Theme::TextPrimary();
			core.a = s.alpha;
			renderer.FillCircle(s.x, s.y, s.size, core);
		}
	}
}

void AetherApp::DrawLogoBadge(float x, float y) {
	if (renderer.pLogoBitmap) {
		// Tint the logo to match the current accent color
		D2D1_COLOR_F tint = Theme::AccentPrimary();
		// Blend accent with text primary for a subtler effect
		tint.r = tint.r * 0.6f + Theme::Base::TextPriR * 0.4f;
		tint.g = tint.g * 0.6f + Theme::Base::TextPriG * 0.4f;
		tint.b = tint.b * 0.6f + Theme::Base::TextPriB * 0.4f;
		tint.a = 1.0f;
		renderer.DrawBitmapTinted(renderer.pLogoBitmap, x - 1.0f, y - 3.0f, 38.0f, 38.0f, tint, 1.0f);
		return;
	}

	// Fallback glyph — also colored by accent
	D2D1_COLOR_F glyphCol = Theme::AccentPrimary();
	glyphCol.r = glyphCol.r * 0.6f + Theme::Base::TextPriR * 0.4f;
	glyphCol.g = glyphCol.g * 0.6f + Theme::Base::TextPriG * 0.4f;
	glyphCol.b = glyphCol.b * 0.6f + Theme::Base::TextPriB * 0.4f;
	renderer.DrawText(L"\x2726", x, y, 32.0f, 32.0f, glyphCol, renderer.pFontTitle, Renderer::AlignCenter);
}

void AetherApp::DrawHeader() {
	float w = Theme::Runtime::WindowWidth;
	float h = Theme::Size::HeaderHeight;

	renderer.FillRect(0, 0, w, h, Theme::BgSurface());
	renderer.DrawLine(0, h, w, h, Theme::BorderSubtle());

	float titleX = Theme::Size::SidebarWidth + 16;
	DrawLogoBadge(titleX, 10);

	renderer.DrawText(L"AETHER", titleX + 42, -1, 170, h, Theme::TextPrimary(), renderer.pFontTitle);

	const wchar_t* tabName = L"Tablet Driver";
	if (sidebar.activeIndex >= 0 && sidebar.activeIndex < (int)sidebar.tabs.size())
		tabName = sidebar.tabs[sidebar.activeIndex].label;
	if (w > 640.0f) {
		renderer.DrawText(tabName, titleX + 135, 0, 160, h, Theme::TextMuted(), renderer.pFontSmall);
	}

	float pillW = 116.0f;
	float buttonW = 96.0f;
	float buttonX = w - buttonW - 14.0f;
	float pillX = buttonX - pillW - 10.0f;
	D2D1_COLOR_F pillBg = driver.isConnected ? Theme::AccentDim() : Theme::BgElevated();
	if (pillX > titleX + 250.0f) {
		renderer.FillRoundedRect(pillX, 11, pillW, 26, 13, pillBg);
		renderer.DrawRoundedRect(pillX, 11, pillW, 26, 13,
			driver.isConnected ? Theme::BorderAccent() : Theme::BorderSubtle(), 1.0f);
		renderer.DrawText(driver.isConnected ? L"\xE73E" : L"\xE895",
			pillX + 10, 11, 18, 26,
			driver.isConnected ? Theme::Success() : Theme::TextSecondary(),
			renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(driver.isConnected ? L"Online" : L"Starting",
			pillX + 32, 11, pillW - 42, 26,
			driver.isConnected ? Theme::Success() : Theme::TextSecondary(),
			renderer.pFontSmall, Renderer::AlignLeft);
	}

	if (w > 820.0f && !displayTargets.empty()) {
		float chipW = 148.0f;
		float chipX = pillX - chipW - 8.0f;
		if (chipX > titleX + 300.0f) {
			std::wstring label = Ellipsize(displayTargets[selectedDisplayTarget].label, 18);
			renderer.FillRoundedRect(chipX, 11, chipW, 26, 13, Theme::BgElevated());
			renderer.DrawRoundedRect(chipX, 11, chipW, 26, 13, Theme::BorderSubtle(), 1.0f);
			renderer.DrawText(L"\xE7F4", chipX + 9, 11, 18, 26, Theme::AccentPrimary(), renderer.pFontIcon, Renderer::AlignCenter);
			renderer.DrawText(label.c_str(), chipX + 31, 11, chipW - 38, 26, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignLeft);
		}
	}

	startStopBtn.x = buttonX;
	startStopBtn.y = 9;
	startStopBtn.width = buttonW;
	startStopBtn.label = driver.isConnected ? L"Stop" : L"Retry";
	startStopBtn.isPrimary = !driver.isConnected;

	if (startStopBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		if (driver.isConnected) {
			driver.Stop();
		} else {
			StartDriverService();
		}
	}
	startStopBtn.Draw(renderer);
}

void AetherApp::DrawOverclockInfo(float x, float y, float w) {
	float h = 60.0f;
	float hz = Clamp(overclockHz.value, 125.0f, 2000.0f);
	float intervalMs = 1000.0f / hz;
	float norm = (hz - 125.0f) / (2000.0f - 125.0f);
	norm = Clamp(norm, 0.0f, 1.0f);

	renderer.FillRoundedRect(x, y, w, h, 6, Theme::BgElevated());
	renderer.DrawRoundedRect(x, y, w, h, 6, Theme::BorderSubtle());

	wchar_t line1[64];
	wchar_t line2[80];
	swprintf_s(line1, L"%s target: %.0f Hz", overclockEnabled.value ? L"Enabled" : L"Disabled", hz);
	swprintf_s(line2, L"Timer %.2f ms   USB/tablet polling can still cap actual rate", intervalMs);

	renderer.DrawText(line1, x + 12, y + 8, w - 24, 16,
		overclockEnabled.value ? Theme::AccentPrimary() : Theme::TextMuted(), renderer.pFontSmall);
	renderer.DrawText(line2, x + 12, y + 34, w - 24, 18, Theme::TextMuted(), renderer.pFontSmall);

	float meterX = x + 12;
	float meterY = y + 27;
	float meterW = w - 24;
	float meterH = 4;
	renderer.FillRoundedRect(meterX, meterY, meterW, meterH, 2, Theme::BgElevated());
	if (norm > 0.01f) {
		renderer.FillRoundedRect(meterX, meterY, meterW * norm, meterH, 2, Theme::AccentPrimary());
		renderer.FillRectGradientH(meterX, meterY, meterW * norm, meterH, Theme::AccentPrimary(), Theme::AccentSecondary());
	}
}

void AetherApp::DrawAreaPanel() {
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = std::max(220.0f, Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2);
	bool areaSingleColumn = cw < 520.0f;
	float hw = areaSingleColumn ? cw : (cw - Theme::Size::Padding) * 0.5f;
	float yStart = Theme::Size::HeaderHeight + Theme::Size::Padding;
	float y = yStart - areaScrollY;

	SectionHeader sec;
	auto drawValueControl = [&](Slider& control, float px, float py, float pw) -> bool {
		control.x = px;
		control.y = py;
		control.width = pw;
		if (area.customValues.value) {
			bool changed = control.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
			control.Draw(renderer);
			return changed;
		}
		bool changed = control.UpdateInput(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
		control.DrawInput(renderer);
		return changed;
	};
	float valueControlStep = area.customValues.value ? 42.0f : 48.0f;

	sec.Layout(cx, y, cw, L"SCREEN MAP");
	wchar_t resBuf[64];
	swprintf_s(resBuf, L"Desktop: %.0fx%.0f", detectedScreenW, detectedScreenH);
	renderer.DrawText(resBuf, cx + cw - 180, y, 180, 18, Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignRight);
	y += sec.Draw(renderer);

	{
		float selectorH = 30.0f;
		float labelX = cx + 42.0f;
		float labelW = std::max(120.0f, cw - 132.0f);

		displayPrevBtn.Layout(cx, y, 32, 28, L"<", false);
		if (displayPrevBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
			ApplyDisplayTarget(selectedDisplayTarget - 1);
		}
		displayPrevBtn.Draw(renderer);

		renderer.FillRoundedRect(labelX, y, labelW, 28, 6, Theme::BgElevated());
		renderer.DrawRoundedRect(labelX, y, labelW, 28, 6, Theme::BorderSubtle());

		const wchar_t* targetLabel = L"Full desktop";
		if (selectedDisplayTarget >= 0 && selectedDisplayTarget < (int)displayTargets.size())
			targetLabel = displayTargets[selectedDisplayTarget].label.c_str();
		renderer.DrawText(L"\xE7F4", labelX + 8, y, 28, 28, Theme::BrandHot(), renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(targetLabel, labelX + 36, y, labelW - 44, 28, Theme::TextPrimary(), renderer.pFontSmall, Renderer::AlignLeft);

		displayNextBtn.Layout(labelX + labelW + 8, y, 32, 28, L">", false);
		if (displayNextBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
			ApplyDisplayTarget(selectedDisplayTarget + 1);
		}
		displayNextBtn.Draw(renderer);

		y += selectorH + 8.0f;
	}

	area.customValues.x = cx;
	area.customValues.y = y;
	area.customValues.Update(mouseX, mouseY, mouseClicked, deltaTime);
	area.customValues.Draw(renderer);
	valueControlStep = area.customValues.value ? 42.0f : 48.0f;
	y += 36.0f;

	{
		float previewW = areaSingleColumn ? (cw - 8) : (hw - 20);
		float previewH = previewW * (detectedScreenH / detectedScreenW);
		float previewX = cx;
		float previewY = y;

		renderer.FillRoundedRect(previewX, previewY, previewW, previewH, 6, Theme::BgElevated());
		renderer.DrawRoundedRect(previewX, previewY, previewW, previewH, 6, Theme::BorderSubtle());

		float totalScreenW = detectedScreenW;
		float scaleX = previewW / totalScreenW;
		float scaleY = previewH / detectedScreenH;
		float scale = (scaleX < scaleY) ? scaleX : scaleY;

		float mappedX = previewX + area.screenX.value * scale;
		float mappedY = previewY + area.screenY.value * scale;
		float mappedW = area.screenWidth.value * scale;
		float mappedH = area.screenHeight.value * scale;

		if (mappedW > previewW) mappedW = previewW;
		if (mappedH > previewH) mappedH = previewH;

		bool hoveringScreenMap = PointInRect(mouseX, mouseY, mappedX, mappedY, mappedW, mappedH);
		if (hoveringScreenMap && mouseClicked && !isDraggingArea) {
			isDraggingArea = true;
			dragTarget = 1;
			dragStartMouseX = mouseX;
			dragStartMouseY = mouseY;
			dragStartValX = area.screenX.value;
			dragStartValY = area.screenY.value;
			dragScale = 1.0f / scale;
		}
		if (isDraggingArea && dragTarget == 1) {
			float dx = (mouseX - dragStartMouseX) * dragScale;
			float dy = (mouseY - dragStartMouseY) * dragScale;
			area.screenX.value = dragStartValX + dx;
			area.screenY.value = dragStartValY + dy;
			if (area.screenX.value < 0) area.screenX.value = 0;
			if (area.screenY.value < 0) area.screenY.value = 0;
		}

		float zoneAlpha = (hoveringScreenMap || (isDraggingArea && dragTarget == 1)) ? 0.34f : 0.22f;
		D2D1_COLOR_F screenHot = Theme::BrandHot();
		D2D1_COLOR_F screenCold = Theme::AccentPrimary();
		screenHot.a = zoneAlpha;
		screenCold.a = zoneAlpha;
		renderer.FillRoundedRect(mappedX, mappedY, mappedW, mappedH, 2, Theme::AccentDim());
		renderer.FillRectGradientH(mappedX, mappedY, mappedW, mappedH, screenHot, screenCold);
		renderer.DrawRoundedRect(mappedX, mappedY, mappedW, mappedH, 2, Theme::BrandHot(), 1.2f);
		renderer.DrawRoundedRect(mappedX + 1, mappedY + 1, mappedW - 2, mappedH - 2, 2, Theme::BorderAccent(), 0.9f);

		float ratio = area.screenWidth.value / area.screenHeight.value;
		wchar_t ratioBuf[32];
		swprintf_s(ratioBuf, L"%.3f:1", ratio);
		renderer.DrawText(ratioBuf, mappedX, mappedY, mappedW, mappedH,
			Theme::TextAccent(), renderer.pFontSmall, Renderer::AlignCenter);

		bool screenChanged = false;
		if (areaSingleColumn) {
			y = previewY + previewH + 12;
			screenChanged |= drawValueControl(area.screenWidth, cx, y, cw);
			y += valueControlStep;
			screenChanged |= drawValueControl(area.screenHeight, cx, y, cw);
			y += valueControlStep;
			if (area.customValues.value) {
				screenChanged |= drawValueControl(area.screenX, cx, y, cw);
				y += valueControlStep;
				screenChanged |= drawValueControl(area.screenY, cx, y, cw);
				y += valueControlStep;
			}
		} else {
			float controlX = cx + hw + 8;
			float controlW = hw - 8;
			screenChanged |= drawValueControl(area.screenWidth, controlX, previewY, controlW);
			screenChanged |= drawValueControl(area.screenHeight, controlX, previewY + valueControlStep, controlW);

			int screenRows = 2;
			if (area.customValues.value) {
				screenChanged |= drawValueControl(area.screenX, controlX, previewY + valueControlStep * 2.0f, controlW);
				screenChanged |= drawValueControl(area.screenY, controlX, previewY + valueControlStep * 3.0f, controlW);
				screenRows = 4;
			}
			float controlsH = valueControlStep * (float)screenRows - 6.0f;
			y = std::max(previewY + previewH, previewY + controlsH) + 16;
		}

		ClampScreenArea();
		if (screenChanged) {
			ApplyAspectLock(false);
			ApplyAllSettings();
		}
	}

	sec.Layout(cx, y, cw, L"TABLET AREA");
	y += sec.Draw(renderer);

	{
		// Use real tablet dimensions from driver if available
		float fullTabletW = (driver.tabletWidth > 1.0f) ? driver.tabletWidth : 152.0f;
		float fullTabletH = (driver.tabletHeight > 1.0f) ? driver.tabletHeight : 95.0f;

		float previewW = hw - 20;
		float previewH = previewW * (fullTabletH / fullTabletW);
		float previewX = cx;
		float previewY = y;

		renderer.FillRoundedRect(previewX, previewY, previewW, previewH, 6, Theme::BgElevated());
		renderer.DrawRoundedRect(previewX, previewY, previewW, previewH, 6, Theme::BorderSubtle());

		float scale = previewW / fullTabletW;
		float areaW = area.tabletWidth.value * scale;
		float areaH = area.tabletHeight.value * scale;

		float centerX = (area.tabletX.value > 0.01f) ? area.tabletX.value : fullTabletW * 0.5f;
		float centerY = (area.tabletY.value > 0.01f) ? area.tabletY.value : fullTabletH * 0.5f;
		float areaX = previewX + (centerX - area.tabletWidth.value * 0.5f) * scale;
		float areaY = previewY + (centerY - area.tabletHeight.value * 0.5f) * scale;

		if (areaW > previewW) areaW = previewW;
		if (areaH > previewH) areaH = previewH;

		bool hoveringTabletArea = PointInRect(mouseX, mouseY, areaX, areaY, areaW, areaH);
		if (hoveringTabletArea && mouseClicked && !isDraggingArea) {
			isDraggingArea = true;
			dragTarget = 2;
			dragStartMouseX = mouseX;
			dragStartMouseY = mouseY;
			dragStartValX = centerX;
			dragStartValY = centerY;
			dragScale = 1.0f / scale;
		}
		if (isDraggingArea && dragTarget == 2) {
			float dx = (mouseX - dragStartMouseX) * dragScale;
			float dy = (mouseY - dragStartMouseY) * dragScale;
			float newX = dragStartValX + dx;
			float newY = dragStartValY + dy;
			float halfW = area.tabletWidth.value * 0.5f;
			float halfH = area.tabletHeight.value * 0.5f;
			if (newX - halfW < 0) newX = halfW;
			if (newY - halfH < 0) newY = halfH;
			if (newX + halfW > fullTabletW) newX = fullTabletW - halfW;
			if (newY + halfH > fullTabletH) newY = fullTabletH - halfH;
			area.tabletX.value = newX;
			area.tabletY.value = newY;
			areaX = previewX + (newX - halfW) * scale;
			areaY = previewY + (newY - halfH) * scale;
		}

		float tabletAlpha = (hoveringTabletArea || (isDraggingArea && dragTarget == 2)) ? 0.34f : 0.22f;
		D2D1_COLOR_F tabHot = Theme::BrandHot();
		D2D1_COLOR_F tabCold = Theme::AccentSecondary();
		tabHot.a = tabletAlpha;
		tabCold.a = tabletAlpha;

		D2D1_MATRIX_3X2_F oldTransform = D2D1::Matrix3x2F::Identity();
		bool canRotatePreview = renderer.pRT != nullptr && areaW > 1.0f && areaH > 1.0f;
		if (canRotatePreview) {
			renderer.pRT->GetTransform(&oldTransform);
			float visualRotation = area.rotation.value;
			D2D1_POINT_2F center = D2D1::Point2F(areaX + areaW * 0.5f, areaY + areaH * 0.5f);
			renderer.pRT->SetTransform(D2D1::Matrix3x2F::Rotation(visualRotation, center) * oldTransform);
		}

		renderer.FillRoundedRect(areaX, areaY, areaW, areaH, 2, Theme::AccentDim());
		renderer.FillRectGradientH(areaX, areaY, areaW, areaH, tabHot, tabCold);
		renderer.DrawRoundedRect(areaX, areaY, areaW, areaH, 2, Theme::BrandHot(), 1.2f);
		renderer.DrawRoundedRect(areaX + 1, areaY + 1, areaW - 2, areaH - 2, 2, Theme::BorderAccent(), 0.9f);

		if (canRotatePreview) {
			renderer.pRT->SetTransform(oldTransform);
		}

		float tabRatio = area.tabletWidth.value / area.tabletHeight.value;
		wchar_t tabRatioBuf[32];
		swprintf_s(tabRatioBuf, L"%.3f:1", tabRatio);
		renderer.DrawText(tabRatioBuf, areaX, areaY, areaW, areaH,
			Theme::AccentSecondary(), renderer.pFontSmall, Renderer::AlignCenter);

		// === Live Cursor Dot ===
		DrawLiveCursor(previewX, previewY, previewW, previewH, fullTabletW, fullTabletH);

		// === Input Visualizer overlay ===
		if (visualizerToggle.value) {
			DrawInputVisualizer(previewX, previewY, previewW, previewH);
		}

		bool tabletChanged = false;
		bool tabletWidthChanged = false;
		bool tabletHeightChanged = false;

		if (areaSingleColumn) {
			y = previewY + previewH + 12;
			tabletWidthChanged = drawValueControl(area.tabletWidth, cx, y, cw);
			y += valueControlStep;
			tabletHeightChanged = drawValueControl(area.tabletHeight, cx, y, cw);
			y += valueControlStep;
			tabletChanged = tabletWidthChanged || tabletHeightChanged;
			if (area.customValues.value) {
				tabletChanged |= drawValueControl(area.tabletX, cx, y, cw);
				y += valueControlStep;
				tabletChanged |= drawValueControl(area.tabletY, cx, y, cw);
				y += valueControlStep;
			}
		} else {
			float controlX = cx + hw + 8;
			float controlW = hw - 8;
			tabletWidthChanged = drawValueControl(area.tabletWidth, controlX, previewY, controlW);
			tabletHeightChanged = drawValueControl(area.tabletHeight, controlX, previewY + valueControlStep, controlW);
			tabletChanged = tabletWidthChanged || tabletHeightChanged;

			int tabletRows = 2;
			if (area.customValues.value) {
				tabletChanged |= drawValueControl(area.tabletX, controlX, previewY + valueControlStep * 2.0f, controlW);
				tabletChanged |= drawValueControl(area.tabletY, controlX, previewY + valueControlStep * 3.0f, controlW);
				tabletRows = 4;
			}

			float controlsH = valueControlStep * (float)tabletRows - 6.0f;
			y = std::max(previewY + previewH, previewY + controlsH) + 16;
		}

		if (tabletWidthChanged || tabletHeightChanged || area.lockAspect.value) {
			ApplyAspectLock(tabletHeightChanged && !tabletWidthChanged);
		}
		if (tabletChanged) {
			ApplyAllSettings();
		}
	}

	sec.Layout(cx, y, cw, L"OPTIONS");
	y += sec.Draw(renderer);

	area.rotation.y = y; area.rotation.x = cx; area.rotation.width = hw - 8;
	if (area.rotation.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) {
		ApplyAllSettings();
	}
	area.rotation.Draw(renderer);

	area.lockAspect.y = y; area.lockAspect.x = cx + hw + 8;
	if (area.lockAspect.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		ApplyAspectLock(false);
		ApplyAllSettings();
	}
	area.lockAspect.Draw(renderer);
	y += 36;

	forceFullArea.y = y; forceFullArea.x = cx;
	if (forceFullArea.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		if (forceFullArea.value) {
			float fW = (driver.tabletWidth > 1.0f) ? driver.tabletWidth : 152.0f;
			float fH = (driver.tabletHeight > 1.0f) ? driver.tabletHeight : 95.25f;
			area.tabletWidth.value = fW;
			area.tabletHeight.value = fH;
			area.tabletX.value = fW * 0.5f;
			area.tabletY.value = fH * 0.5f;
			ApplyAspectLock(false);
		}
		ApplyAllSettings();
	}
	forceFullArea.Draw(renderer);

	areaClipping.y = y; areaClipping.x = cx + hw + 8;
	if (areaClipping.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		ApplyAllSettings();
	}
	areaClipping.Draw(renderer);
	y += 36;

	areaLimiting.y = y; areaLimiting.x = cx;
	if (areaLimiting.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		if (areaLimiting.value) areaClipping.value = true;
		ApplyAllSettings();
	}
	areaLimiting.Draw(renderer);

	visualizerToggle.y = y; visualizerToggle.x = cx + hw + 8;
	visualizerToggle.Update(mouseX, mouseY, mouseClicked, deltaTime);
	visualizerToggle.Draw(renderer);
	y += 36;

	sec.Layout(cx, y, cw, L"OUTPUT MODE");
	y += sec.Draw(renderer);

	outputMode.x = cx; outputMode.y = y;
	float modeBtnW = (cw - 8) / 3.0f;
	if (outputMode.Update(mouseX, mouseY, mouseClicked, deltaTime, modeBtnW) >= 0) {
		ApplyAllSettings();
	}
	outputMode.Draw(renderer, modeBtnW);

	// Warning if Windows Ink selected but VMulti not installed
	if (outputMode.selected == 2 && vmultiCheckDone && !vmultiInstalled) {
		y += 32;
		renderer.FillRoundedRect(cx, y, cw, 28, 6, D2D1::ColorF(0.9f, 0.35f, 0.2f, 0.12f));
		renderer.DrawRoundedRect(cx, y, cw, 28, 6, D2D1::ColorF(0.9f, 0.35f, 0.2f, 0.3f));
		renderer.DrawText(L"\xE7BA", cx + 10, y, 18, 28, Theme::Warning(), renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(L"Windows Ink requires VMulti driver. Install VMulti to use this mode.",
			cx + 32, y, cw - 44, 28, Theme::Warning(), renderer.pFontSmall);
	}
	y += 36;

	float btnW = 100.0f;
	float btnGap = 8.0f;
	float btnsX = cx + cw - (btnW * 2 + btnGap);
	saveConfigBtn.Layout(btnsX, y, btnW, 28, L"Save Config", false);
	if (saveConfigBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		AutoSaveConfig();
	}
	saveConfigBtn.Draw(renderer);

	loadConfigBtn.Layout(btnsX + btnW + btnGap, y, btnW, 28, L"Load Config", false);
	if (loadConfigBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		AutoLoadConfig();
	}
	loadConfigBtn.Draw(renderer);
	y += 40;

	areaContentH = (y + areaScrollY) - yStart + 40;
}

void AetherApp::DrawSettingsPanel() {
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2;
	float hw = (cw - Theme::Size::Padding) * 0.5f;
	float yStart = Theme::Size::HeaderHeight + Theme::Size::Padding;
	float y = yStart - settingsScrollY;

	SectionHeader sec;

	sec.Layout(cx, y, cw, L"BUTTON MAPPING");
	y += sec.Draw(renderer);

	buttonTip.x = cx; buttonTip.y = y; buttonTip.width = hw;
	if (buttonTip.Update(mouseX, mouseY, mouseClicked, deltaTime)) ApplyAllSettings();
	buttonTip.Draw(renderer);
	buttonBottom.x = cx + hw + 16; buttonBottom.y = y; buttonBottom.width = hw;
	if (buttonBottom.Update(mouseX, mouseY, mouseClicked, deltaTime)) ApplyAllSettings();
	buttonBottom.Draw(renderer);
	y += 34;
	buttonTop.x = cx; buttonTop.y = y; buttonTop.width = hw;
	if (buttonTop.Update(mouseX, mouseY, mouseClicked, deltaTime)) ApplyAllSettings();
	buttonTop.Draw(renderer);
	y += 44;

	sec.Layout(cx, y, cw, L"PEN SETTINGS");
	y += sec.Draw(renderer);

	tipThreshold.y = y; tipThreshold.x = cx; tipThreshold.width = hw - 8;
	if (tipThreshold.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) ApplyAllSettings();
	tipThreshold.Draw(renderer);
	y += 48;

	sec.Layout(cx, y, cw, L"THEME"); y += sec.Draw(renderer);
	DrawThemeSelector(cx, y, cw);

	sec.Layout(cx, y, cw, L"PARTICLES"); y += sec.Draw(renderer);
	{
		const wchar_t* particleNames[] = { L"Stars", L"Fireflies", L"Snow", L"None" };
		int numStyles = 4;
		float pbtnW = (cw - (numStyles - 1) * 4.0f) / (float)numStyles;
		for (int i = 0; i < numStyles; i++) {
			float bx = cx + i * (pbtnW + 4.0f);
			bool isActive = (i == particleStyle);
			bool hovered = PointInRect(mouseX, mouseY, bx, y, pbtnW, 26);
			if (isActive) {
				renderer.FillRoundedRect(bx, y, pbtnW, 26, 6, Theme::AccentPrimary());
				renderer.DrawText(particleNames[i], bx, y, pbtnW, 26, D2D1::ColorF(0xFFFFFF), renderer.pFontSmall, Renderer::AlignCenter);
			} else {
				D2D1_COLOR_F bg = hovered ? Theme::BgHover() : Theme::BgElevated();
				renderer.FillRoundedRect(bx, y, pbtnW, 26, 6, bg);
				renderer.DrawRoundedRect(bx, y, pbtnW, 26, 6, Theme::BorderSubtle());
				D2D1_COLOR_F tc = hovered ? Theme::TextPrimary() : Theme::TextMuted();
				renderer.DrawText(particleNames[i], bx, y, pbtnW, 26, tc, renderer.pFontSmall, Renderer::AlignCenter);
			}
			if (hovered && mouseClicked) {
				particleStyle = i;
				AutoSaveConfig();
			}
		}
		y += 38;
	}

	sec.Layout(cx, y, cw, L"PROFILES"); y += sec.Draw(renderer);
	DrawProfileSelector(cx, y, cw);
	y += 44;

	settingsContentH = (y + settingsScrollY) - yStart + 40;
}

void AetherApp::SaveConfig(const std::wstring& path) {
	std::ofstream f(path);
	if (!f.is_open()) return;

	f << "# Aether Tablet Driver Config\n";
	f << "TabletWidth=" << area.tabletWidth.value << "\n";
	f << "TabletHeight=" << area.tabletHeight.value << "\n";
	f << "TabletX=" << area.tabletX.value << "\n";
	f << "TabletY=" << area.tabletY.value << "\n";
	f << "ScreenWidth=" << area.screenWidth.value << "\n";
	f << "ScreenHeight=" << area.screenHeight.value << "\n";
	f << "ScreenX=" << area.screenX.value << "\n";
	f << "ScreenY=" << area.screenY.value << "\n";
	f << "Rotation=" << area.rotation.value << "\n";
	f << "OutputMode=" << outputMode.selected << "\n";
	f << "ButtonTip=" << buttonTip.selected << "\n";
	f << "ButtonBottom=" << buttonBottom.selected << "\n";
	f << "ButtonTop=" << buttonTop.selected << "\n";
	f << "ForceFullArea=" << (int)forceFullArea.value << "\n";
	f << "AreaClipping=" << (int)areaClipping.value << "\n";
	f << "AreaLimiting=" << (int)areaLimiting.value << "\n";
	f << "TipThreshold=" << tipThreshold.value << "\n";
	f << "Overclock=" << (int)overclockEnabled.value << "\n";
	f << "OverclockHz=" << overclockHz.value << "\n";
	f << "LockAspect=" << (int)area.lockAspect.value << "\n";
	f << "AccentR=" << Theme::Custom::AccentR << "\n";
	f << "AccentG=" << Theme::Custom::AccentG << "\n";
	f << "AccentB=" << Theme::Custom::AccentB << "\n";
	f << "UITheme=" << currentTheme << "\n";

	f << "\n";
	f << "SmoothingEnabled=" << (int)filters.smoothingEnabled.value << "\n";
	f << "SmoothingLatency=" << filters.smoothingLatency.value << "\n";
	f << "SmoothingInterval=" << filters.smoothingInterval.value << "\n";

	f << "AntichatterEnabled=" << (int)filters.antichatterEnabled.value << "\n";
	f << "AntichatterStrength=" << filters.antichatterStrength.value << "\n";
	f << "AntichatterMultiplier=" << filters.antichatterMultiplier.value << "\n";
	f << "AntichatterOffsetX=" << filters.antichatterOffsetX.value << "\n";
	f << "AntichatterOffsetY=" << filters.antichatterOffsetY.value << "\n";

	f << "NoiseEnabled=" << (int)filters.noiseEnabled.value << "\n";
	f << "NoiseBuffer=" << filters.noiseBuffer.value << "\n";
	f << "NoiseThreshold=" << filters.noiseThreshold.value << "\n";
	f << "NoiseIterations=" << filters.noiseIterations.value << "\n";

	f << "VelCurveEnabled=" << (int)filters.velCurveEnabled.value << "\n";
	f << "VelCurveMinSpeed=" << filters.velCurveMinSpeed.value << "\n";
	f << "VelCurveMaxSpeed=" << filters.velCurveMaxSpeed.value << "\n";
	f << "VelCurveSmoothing=" << filters.velCurveSmoothing.value << "\n";
	f << "VelCurveSharpness=" << filters.velCurveSharpness.value << "\n";

	f << "SnapEnabled=" << (int)filters.snapEnabled.value << "\n";
	f << "SnapRadius=" << filters.snapRadius.value << "\n";
	f << "SnapSmooth=" << filters.snapSmooth.value << "\n";

	f << "ReconstructorEnabled=" << (int)filters.reconstructorEnabled.value << "\n";
	f << "ReconStrength=" << filters.reconStrength.value << "\n";
	f << "ReconVelSmooth=" << filters.reconVelSmooth.value << "\n";
	f << "ReconAccelCap=" << filters.reconAccelCap.value << "\n";
	f << "ReconPredTime=" << filters.reconPredTime.value << "\n";

	f << "AdaptiveEnabled=" << (int)filters.adaptiveEnabled.value << "\n";
	f << "AdaptiveProcessNoise=" << filters.adaptiveProcessNoise.value << "\n";
	f << "AdaptiveMeasNoise=" << filters.adaptiveMeasNoise.value << "\n";
	f << "AdaptiveVelWeight=" << filters.adaptiveVelWeight.value << "\n";

	f << "\n";
	f << "AetherEnabled=" << (int)aether.enabled.value << "\n";
	f << "AetherLagRemoval=" << (int)aether.lagRemovalEnabled.value << "\n";
	f << "AetherLagStrength=" << aether.lagRemovalStrength.value << "\n";
	f << "AetherStabilizer=" << (int)aether.stabilizerEnabled.value << "\n";
	f << "AetherStability=" << aether.stabilizerStability.value << "\n";
	f << "AetherSensitivity=" << aether.stabilizerSensitivity.value << "\n";
	f << "AetherSnapping=" << (int)aether.snappingEnabled.value << "\n";
	f << "AetherSnapInner=" << aether.snappingInner.value << "\n";
	f << "AetherSnapOuter=" << aether.snappingOuter.value << "\n";
	f << "AetherSuppression=" << (int)aether.suppressionEnabled.value << "\n";
	f << "AetherSuppressTime=" << aether.suppressionTime.value << "\n";
	f << "Visualizer=" << (int)visualizerToggle.value << "\n";
	f << "ParticleStyle=" << particleStyle << "\n";

	f.close();
}

void AetherApp::LoadConfig(const std::wstring& path) {
	std::ifstream f(path);
	if (!f.is_open()) return;

	std::string line;
	while (std::getline(f, line)) {
		if (line.empty() || line[0] == '#') continue;
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string key = line.substr(0, eq);
		float val = std::stof(line.substr(eq + 1));

		if (key == "TabletWidth") area.tabletWidth.value = val;
		else if (key == "TabletHeight") area.tabletHeight.value = val;
		else if (key == "TabletX") area.tabletX.value = val;
		else if (key == "TabletY") area.tabletY.value = val;
		else if (key == "ScreenWidth") area.screenWidth.value = val;
		else if (key == "ScreenHeight") area.screenHeight.value = val;
		else if (key == "ScreenX") area.screenX.value = val;
		else if (key == "ScreenY") area.screenY.value = val;
		else if (key == "Rotation") area.rotation.value = val;
		else if (key == "OutputMode") outputMode.selected = (int)val;
		else if (key == "ButtonTip") buttonTip.selected = (int)val;
		else if (key == "ButtonBottom") buttonBottom.selected = (int)val;
		else if (key == "ButtonTop") buttonTop.selected = (int)val;
		else if (key == "ForceFullArea") forceFullArea.value = (val > 0.5f);
		else if (key == "AreaClipping") areaClipping.value = (val > 0.5f);
		else if (key == "AreaLimiting") areaLimiting.value = (val > 0.5f);
		else if (key == "TipThreshold") tipThreshold.value = val;
		else if (key == "Overclock") overclockEnabled.value = (val > 0.5f);
		else if (key == "OverclockHz") overclockHz.value = val;
		else if (key == "LockAspect") area.lockAspect.value = (val > 0.5f);
		else if (key == "AccentR") Theme::Custom::AccentR = val;
		else if (key == "AccentG") Theme::Custom::AccentG = val;
		else if (key == "AccentB") Theme::Custom::AccentB = val;
		else if (key == "UITheme") {
			int idx = (int)val;
			if (idx < 0 || idx >= uiThemeCount) idx = 0; // fallback to Midnight if out of range
			currentTheme = idx;
			Theme::ApplyTheme(uiThemes[idx]);
		}
		else if (key == "SmoothingEnabled") filters.smoothingEnabled.value = (val > 0.5f);
		else if (key == "SmoothingLatency") filters.smoothingLatency.value = val;
		else if (key == "SmoothingInterval") filters.smoothingInterval.value = val;
		else if (key == "AntichatterEnabled") filters.antichatterEnabled.value = (val > 0.5f);
		else if (key == "AntichatterStrength") filters.antichatterStrength.value = val;
		else if (key == "AntichatterMultiplier") filters.antichatterMultiplier.value = val;
		else if (key == "AntichatterOffsetX") filters.antichatterOffsetX.value = val;
		else if (key == "AntichatterOffsetY") filters.antichatterOffsetY.value = val;
		else if (key == "NoiseEnabled") filters.noiseEnabled.value = (val > 0.5f);
		else if (key == "NoiseBuffer") filters.noiseBuffer.value = val;
		else if (key == "NoiseThreshold") filters.noiseThreshold.value = val;
		else if (key == "NoiseIterations") filters.noiseIterations.value = val;
		else if (key == "VelCurveEnabled") filters.velCurveEnabled.value = (val > 0.5f);
		else if (key == "VelCurveMinSpeed") filters.velCurveMinSpeed.value = val;
		else if (key == "VelCurveMaxSpeed") filters.velCurveMaxSpeed.value = val;
		else if (key == "VelCurveSmoothing") filters.velCurveSmoothing.value = val;
		else if (key == "VelCurveSharpness") filters.velCurveSharpness.value = val;

		else if (key == "SnapEnabled") filters.snapEnabled.value = (val > 0.5f);
		else if (key == "SnapRadius") filters.snapRadius.value = val;
		else if (key == "SnapSmooth") filters.snapSmooth.value = val;
		else if (key == "ReconstructorEnabled") filters.reconstructorEnabled.value = (val > 0.5f);
		else if (key == "ReconStrength") filters.reconStrength.value = val;
		else if (key == "ReconVelSmooth") filters.reconVelSmooth.value = val;
		else if (key == "ReconAccelCap") filters.reconAccelCap.value = val;
		else if (key == "ReconPredTime") filters.reconPredTime.value = val;
		else if (key == "AdaptiveEnabled") filters.adaptiveEnabled.value = (val > 0.5f);
		else if (key == "AdaptiveProcessNoise") filters.adaptiveProcessNoise.value = val;
		else if (key == "AdaptiveMeasNoise") filters.adaptiveMeasNoise.value = val;
		else if (key == "AdaptiveVelWeight") filters.adaptiveVelWeight.value = val;
		else if (key == "AetherEnabled") aether.enabled.value = (val > 0.5f);
		else if (key == "AetherLagRemoval") aether.lagRemovalEnabled.value = (val > 0.5f);
		else if (key == "AetherLagStrength") aether.lagRemovalStrength.value = val;
		else if (key == "AetherStabilizer") aether.stabilizerEnabled.value = (val > 0.5f);
		else if (key == "AetherStability") aether.stabilizerStability.value = val;
		else if (key == "AetherSensitivity") aether.stabilizerSensitivity.value = val;
		else if (key == "AetherSnapping") aether.snappingEnabled.value = (val > 0.5f);
		else if (key == "AetherSnapInner") aether.snappingInner.value = val;
		else if (key == "AetherSnapOuter") aether.snappingOuter.value = val;
		else if (key == "AetherSuppression") aether.suppressionEnabled.value = (val > 0.5f);
		else if (key == "AetherSuppressTime") aether.suppressionTime.value = val;
		else if (key == "Visualizer") visualizerToggle.value = (val > 0.5f);
		else if (key == "ParticleStyle") { particleStyle = (int)val; if (particleStyle < 0 || particleStyle > 3) particleStyle = 0; }
	}
	f.close();
	ClampScreenArea();
	ApplyAspectLock(false);
	accentPicker.SetRGB(Theme::Custom::AccentR, Theme::Custom::AccentG, Theme::Custom::AccentB);
	// Sync hex input
	{
		wchar_t hexBuf[16];
		swprintf_s(hexBuf, L"#%02X%02X%02X",
			(int)(Theme::Custom::AccentR * 255),
			(int)(Theme::Custom::AccentG * 255),
			(int)(Theme::Custom::AccentB * 255));
		wcscpy_s(hexColorInput.buffer, hexBuf);
		hexColorInput.cursor = (int)wcslen(hexBuf);
	}
}
void AetherApp::DrawFilterPanel() {
	bool filterChanged = false;
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = std::max(220.0f, Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2);
	bool filterSingleColumn = cw < 640.0f;
	float filterGap = filterSingleColumn ? 0.0f : 16.0f;
	float hw = filterSingleColumn ? cw : (cw - filterGap) * 0.5f;
	float filterRightX = filterSingleColumn ? cx : cx + hw + filterGap;
	float yStart = Theme::Size::HeaderHeight + Theme::Size::Padding;
	float y = yStart - filterScrollY;

	SectionHeader sec;
	Slider* filterSliders[] = {
		&filters.smoothingLatency, &filters.smoothingInterval,
		&filters.antichatterStrength, &filters.antichatterMultiplier,
		&filters.antichatterOffsetX, &filters.antichatterOffsetY,
		&filters.noiseBuffer, &filters.noiseThreshold, &filters.noiseIterations,
		&filters.velCurveMinSpeed, &filters.velCurveMaxSpeed,
		&filters.velCurveSmoothing, &filters.velCurveSharpness,
		&filters.snapRadius, &filters.snapSmooth,
		&filters.reconStrength, &filters.reconVelSmooth,
		&filters.reconAccelCap, &filters.reconPredTime,
		&filters.adaptiveProcessNoise, &filters.adaptiveMeasNoise, &filters.adaptiveVelWeight
	};
	for (Slider* slider : filterSliders) {
		slider->width = hw;
	}

	sec.Layout(cx, y, cw, L"SMOOTHING"); y += sec.Draw(renderer);
	filters.smoothingEnabled.y = y; filters.smoothingEnabled.x = cx;
	filterChanged |= filters.smoothingEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); filters.smoothingEnabled.Draw(renderer); y += 30;

	if (filters.smoothingEnabled.value) {
		filters.smoothingLatency.y = y; filters.smoothingLatency.x = cx;
		filterChanged |= filters.smoothingLatency.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.smoothingLatency.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.smoothingInterval.y = y; filters.smoothingInterval.x = filterRightX;
		filterChanged |= filters.smoothingInterval.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.smoothingInterval.Draw(renderer);
		y += 48;
	}

	sec.Layout(cx, y, cw, L"ANTICHATTER"); y += sec.Draw(renderer);
	filters.antichatterEnabled.y = y; filters.antichatterEnabled.x = cx;
	filterChanged |= filters.antichatterEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); filters.antichatterEnabled.Draw(renderer);
	if (!filters.smoothingEnabled.value) {
		float warnX = cx + Theme::Size::ToggleWidth + 120;
		renderer.DrawText(L"\xE7BA", warnX, y + 3, 16, 16, Theme::Warning(), renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(L"Requires Smoothing", warnX + 20, y, 200, Theme::Size::ToggleHeight, Theme::Warning(), renderer.pFontSmall);
	}
	y += 30;

	if (filters.antichatterEnabled.value) {
		filters.antichatterStrength.y = y; filters.antichatterStrength.x = cx;
		filterChanged |= filters.antichatterStrength.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.antichatterStrength.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.antichatterMultiplier.y = y; filters.antichatterMultiplier.x = filterRightX;
		filterChanged |= filters.antichatterMultiplier.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.antichatterMultiplier.Draw(renderer);
		y += 48;
		filters.antichatterOffsetX.y = y; filters.antichatterOffsetX.x = cx;
		filterChanged |= filters.antichatterOffsetX.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.antichatterOffsetX.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.antichatterOffsetY.y = y; filters.antichatterOffsetY.x = filterRightX;
		filterChanged |= filters.antichatterOffsetY.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.antichatterOffsetY.Draw(renderer);
		y += 48;
	}

	sec.Layout(cx, y, cw, L"NOISE REDUCTION"); y += sec.Draw(renderer);
	filters.noiseEnabled.y = y; filters.noiseEnabled.x = cx;
	filterChanged |= filters.noiseEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); filters.noiseEnabled.Draw(renderer); y += 30;

	if (filters.noiseEnabled.value) {
		filters.noiseBuffer.y = y; filters.noiseBuffer.x = cx;
		filterChanged |= filters.noiseBuffer.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.noiseBuffer.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.noiseThreshold.y = y; filters.noiseThreshold.x = filterRightX;
		filterChanged |= filters.noiseThreshold.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.noiseThreshold.Draw(renderer);
		y += 48;
		filters.noiseIterations.y = y; filters.noiseIterations.x = cx;
		filterChanged |= filters.noiseIterations.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.noiseIterations.Draw(renderer);
		y += 48;
	}





	sec.Layout(cx, y, cw, L"RECONSTRUCTOR"); y += sec.Draw(renderer);
	filters.reconstructorEnabled.y = y; filters.reconstructorEnabled.x = cx;
	filterChanged |= filters.reconstructorEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); filters.reconstructorEnabled.Draw(renderer); y += 30;

	if (filters.reconstructorEnabled.value) {
		// Warning banner
		renderer.FillRoundedRect(cx, y, cw, 24, 6, D2D1::ColorF(0.85f, 0.2f, 0.2f, 0.10f));
		renderer.DrawRoundedRect(cx, y, cw, 24, 6, D2D1::ColorF(0.85f, 0.2f, 0.2f, 0.25f));
		renderer.DrawText(L"\xE7BA", cx + 8, y, 16, 24, Theme::Error(), renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(L"High values may cause instability. Use at your own risk.",
			cx + 28, y, cw - 36, 24, Theme::Error(), renderer.pFontSmall);
		y += 30;
		filters.reconStrength.y = y; filters.reconStrength.x = cx;
		filterChanged |= filters.reconStrength.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.reconStrength.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.reconVelSmooth.y = y; filters.reconVelSmooth.x = filterRightX;
		filterChanged |= filters.reconVelSmooth.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.reconVelSmooth.Draw(renderer);
		y += 48;
		filters.reconAccelCap.y = y; filters.reconAccelCap.x = cx;
		filterChanged |= filters.reconAccelCap.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.reconAccelCap.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.reconPredTime.y = y; filters.reconPredTime.x = filterRightX;
		filterChanged |= filters.reconPredTime.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.reconPredTime.Draw(renderer);
		y += 48;
	}

	sec.Layout(cx, y, cw, L"ADAPTIVE FILTER"); y += sec.Draw(renderer);
	filters.adaptiveEnabled.y = y; filters.adaptiveEnabled.x = cx;
	filterChanged |= filters.adaptiveEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); filters.adaptiveEnabled.Draw(renderer); y += 30;

	if (filters.adaptiveEnabled.value) {
		filters.adaptiveProcessNoise.y = y; filters.adaptiveProcessNoise.x = cx;
		filterChanged |= filters.adaptiveProcessNoise.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.adaptiveProcessNoise.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.adaptiveMeasNoise.y = y; filters.adaptiveMeasNoise.x = filterRightX;
		filterChanged |= filters.adaptiveMeasNoise.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.adaptiveMeasNoise.Draw(renderer);
		y += 48;
		filters.adaptiveVelWeight.y = y; filters.adaptiveVelWeight.x = cx;
		filterChanged |= filters.adaptiveVelWeight.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.adaptiveVelWeight.Draw(renderer);
		y += 48;
	}

	// === AETHER SMOOTH ===
	sec.Layout(cx, y, cw, L"AETHER SMOOTH"); y += sec.Draw(renderer);
	aether.enabled.y = y; aether.enabled.x = cx;
	filterChanged |= aether.enabled.Update(mouseX, mouseY, mouseClicked, deltaTime); aether.enabled.Draw(renderer); y += 30;

	if (aether.enabled.value) {
		// Lag Removal
		aether.lagRemovalEnabled.y = y; aether.lagRemovalEnabled.x = cx;
		filterChanged |= aether.lagRemovalEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); aether.lagRemovalEnabled.Draw(renderer); y += 28;
		if (aether.lagRemovalEnabled.value) {
			aether.lagRemovalStrength.y = y; aether.lagRemovalStrength.x = cx; aether.lagRemovalStrength.width = hw;
			filterChanged |= aether.lagRemovalStrength.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.lagRemovalStrength.Draw(renderer);
			y += 48;
		}

		// Stabilizer
		aether.stabilizerEnabled.y = y; aether.stabilizerEnabled.x = cx;
		filterChanged |= aether.stabilizerEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); aether.stabilizerEnabled.Draw(renderer); y += 28;
		if (aether.stabilizerEnabled.value) {
			aether.stabilizerStability.y = y; aether.stabilizerStability.x = cx; aether.stabilizerStability.width = hw;
			filterChanged |= aether.stabilizerStability.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.stabilizerStability.Draw(renderer);
			if (filterSingleColumn) y += 48;
			aether.stabilizerSensitivity.y = y; aether.stabilizerSensitivity.x = filterRightX; aether.stabilizerSensitivity.width = hw;
			filterChanged |= aether.stabilizerSensitivity.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.stabilizerSensitivity.Draw(renderer);
			y += 48;
		}

		// Snapping
		aether.snappingEnabled.y = y; aether.snappingEnabled.x = cx;
		filterChanged |= aether.snappingEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); aether.snappingEnabled.Draw(renderer); y += 28;
		if (aether.snappingEnabled.value) {
			aether.snappingInner.y = y; aether.snappingInner.x = cx; aether.snappingInner.width = hw;
			filterChanged |= aether.snappingInner.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.snappingInner.Draw(renderer);
			if (filterSingleColumn) y += 48;
			aether.snappingOuter.y = y; aether.snappingOuter.x = filterRightX; aether.snappingOuter.width = hw;
			filterChanged |= aether.snappingOuter.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.snappingOuter.Draw(renderer);
			y += 48;
		}

		// Suppression
		aether.suppressionEnabled.y = y; aether.suppressionEnabled.x = cx;
		filterChanged |= aether.suppressionEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); aether.suppressionEnabled.Draw(renderer); y += 28;
		if (aether.suppressionEnabled.value) {
			aether.suppressionTime.y = y; aether.suppressionTime.x = cx; aether.suppressionTime.width = hw;
			filterChanged |= aether.suppressionTime.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.suppressionTime.Draw(renderer);
			y += 48;
		}
	}

	// === OVERCLOCK ===
	sec.Layout(cx, y, cw, L"OVERCLOCK"); y += sec.Draw(renderer);
	overclockEnabled.y = y; overclockEnabled.x = cx;
	if (overclockEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime)) filterChanged = true;
	overclockEnabled.Draw(renderer);
	if (!filters.smoothingEnabled.value) {
		float warnX = cx + Theme::Size::ToggleWidth + 120;
		renderer.DrawText(L"\xE7BA", warnX, y + 3, 16, 16, Theme::Warning(), renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(L"Requires Smoothing", warnX + 20, y, 200, Theme::Size::ToggleHeight, Theme::Warning(), renderer.pFontSmall);
	}
	y += 34;

	if (overclockEnabled.value) {
		overclockHz.y = y; overclockHz.x = cx; overclockHz.width = hw;
		if (overclockHz.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) filterChanged = true;
		overclockHz.Draw(renderer);
		y += 42;

		DrawOverclockInfo(cx, y, cw);
		y += 76;
	}

	if (filterChanged) {
		ApplyAllSettings();
	}

	filterContentH = (y + filterScrollY) - yStart;
}

void AetherApp::DrawConsolePanel() {
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2;
	float y = Theme::Size::HeaderHeight + Theme::Size::Padding;
	float inputH = 36;
	float consoleH = Theme::Runtime::WindowHeight - y - 50 - inputH;

	renderer.FillRoundedRect(cx, y, cw, consoleH, Theme::Size::CornerRadius, Theme::BgElevated());
	renderer.DrawRoundedRect(cx, y, cw, consoleH, Theme::Size::CornerRadius, Theme::BorderSubtle());

	auto lines = driver.GetLogLines();
	float lineH = 16;
	float textY = y + 8;
	int maxVisible = (int)(consoleH / lineH) - 1;
	int startIdx = (int)lines.size() - maxVisible;
	if (startIdx < 0) startIdx = 0;

	for (int i = startIdx; i < (int)lines.size(); i++) {
		if (textY > y + consoleH - 8) break;

		std::wstring wline(lines[i].begin(), lines[i].end());
		D2D1_COLOR_F color = Theme::TextSecondary();

		if (lines[i].find("[ERROR]") != std::string::npos) color = Theme::Error();
		else if (lines[i].find("[WARNING]") != std::string::npos) color = Theme::Warning();
		else if (lines[i].find("[STATUS]") != std::string::npos) color = Theme::AccentPrimary();

		renderer.DrawText(wline.c_str(), cx + 12, textY, cw - 24, lineH, color, renderer.pFontMono);
		textY += lineH;
	}

	if (lines.empty()) {
		renderer.DrawText(L"No output \x2014 start the driver to see logs here",
			cx + 12, y + consoleH * 0.5f - 10, cw - 24, 20,
			Theme::TextMuted(), renderer.pFontBody, Renderer::AlignCenter);
	}

	float inputY = y + consoleH + 6;
	consoleInput.x = cx; consoleInput.y = inputY; consoleInput.width = cw;
	consoleInput.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
	consoleInput.Draw(renderer);
}

void AetherApp::DrawAboutPanel() {
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2;
	float y = Theme::Size::HeaderHeight + 38;

	aboutAnimT += deltaTime;
	float logoSize = 38.0f;
	float centerX = cx + cw * 0.5f - 1.0f;
	float floatY = y + sinf(aboutAnimT * 1.2f) * 4.0f;
	float logoX = centerX - logoSize * 0.5f;
	float logoY = floatY;
	float breath = 1.0f + 0.03f * sinf(aboutAnimT * 1.8f);

	D2D1_POINT_2F logoCtr = D2D1::Point2F(centerX, floatY + logoSize * 0.5f - 3.0f);

	D2D1_COLOR_F glowOuter = Theme::AccentGlow();
	glowOuter.a = 0.08f + 0.06f * sinf(aboutAnimT * 1.8f);
	renderer.DrawCircle(logoCtr.x, logoCtr.y, 26.0f * breath, glowOuter, 1.0f);

	D2D1_COLOR_F glowInner = Theme::BrandHot();
	glowInner.a = 0.10f + 0.08f * sinf(aboutAnimT * 2.2f + 0.5f);
	renderer.FillCircle(logoCtr.x, logoCtr.y, 20.0f * breath, glowInner);

	for (int i = 0; i < 3; i++) {
		float angle = aboutAnimT * 0.8f + i * 2.094f;
		float orbitR = 24.0f * breath;
		float dotX = logoCtr.x + cosf(angle) * orbitR;
		float dotY = logoCtr.y + sinf(angle) * orbitR;
		float dotAlpha = 0.3f + 0.2f * sinf(aboutAnimT * 2.5f + i * 1.0f);
		D2D1_COLOR_F dotColor = Theme::AccentSecondary();
		dotColor.a = dotAlpha;
		renderer.FillCircle(dotX, dotY, 2.0f, dotColor);
	}

	D2D1_MATRIX_3X2_F oldTransform;
	renderer.pRT->GetTransform(&oldTransform);
	renderer.pRT->SetTransform(
		D2D1::Matrix3x2F::Scale(breath, breath, logoCtr) * oldTransform);
	DrawLogoBadge(logoX, logoY);
	renderer.pRT->SetTransform(oldTransform);

	y += 36;
	renderer.DrawText(L"AETHER", cx, y, cw, 40, Theme::TextPrimary(), renderer.pFontTitle, Renderer::AlignCenter);
	renderer.FillRectGradientH(cx + cw * 0.5f - 72.0f, y + 39.0f, 144.0f, 2.0f, Theme::BrandHot(), Theme::AccentPrimary());
	y += 54;
	renderer.DrawText(L"High-performance tablet driver for creative workflows", cx, y, cw, 20, Theme::TextSecondary(), renderer.pFontBody, Renderer::AlignCenter);
	y += 30;
	renderer.DrawText(L"Made by q1xlf (known as sophia)", cx, y, cw, 20, Theme::TextAccent(), renderer.pFontSmall, Renderer::AlignCenter);
	y += 50;

	renderer.DrawText(L"Filters: Smoothing \x2022 Antichatter \x2022 Noise Reduction", cx, y, cw, 18, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignCenter);
	y += 22;
	renderer.DrawText(L"Reconstructor \x2022 Adaptive Filter \x2022 Aether Smooth", cx, y, cw, 18, Theme::TextAccent(), renderer.pFontSmall, Renderer::AlignCenter);
	y += 22;
	renderer.DrawText(L"Live Cursor \x2022 Input Visualizer \x2022 Theme Presets \x2022 Profiles", cx, y, cw, 18, Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignCenter);
}

void AetherApp::DrawStatusBar() {
	float w = Theme::Runtime::WindowWidth;
	float h = 28;
	float y = Theme::Runtime::WindowHeight - h;

	renderer.FillRect(0, y, w, h, Theme::BgSurface());
	renderer.DrawLine(0, y, w, y, Theme::BorderSubtle());

	D2D1_COLOR_F statusColor = driver.isConnected ? Theme::Success() : Theme::TextMuted();
	renderer.FillCircle(18, y + h * 0.5f, 3.0f, statusColor);
	const wchar_t* statusText = driver.isConnected ? L"Connected" : L"Not connected";
	renderer.DrawText(statusText, 28, y, 120, h, statusColor, renderer.pFontSmall);

	if (driver.isConnected) {
		std::wstring name(driver.tabletName.begin(), driver.tabletName.end());
		renderer.DrawText(name.c_str(), 150, y, 240, h, Theme::TextMuted(), renderer.pFontSmall);
	}

	if (w > 560.0f) {
		const wchar_t* modes[] = { L"Absolute", L"Relative", L"Windows Ink" };
		const wchar_t* mode = modes[(outputMode.selected >= 0 && outputMode.selected < 3) ? outputMode.selected : 0];
		renderer.DrawText(L"Mode", w - 330, y, 44, h, Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignRight);
		renderer.DrawText(mode, w - 280, y, 86, h, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignLeft);
	}

	if (w > 720.0f) {
		wchar_t hzText[64];
		swprintf_s(hzText, L"%.0f Hz", overclockHz.value);
		renderer.DrawText(L"Overclock", w - 178, y, 74, h, Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignRight);
		renderer.DrawText(overclockEnabled.value ? hzText : L"off", w - 96, y, 82, h,
			overclockEnabled.value ? Theme::AccentPrimary() : Theme::TextMuted(),
			renderer.pFontSmall, Renderer::AlignLeft);
	}

	// Live Hz meter
	UpdateHzMeter();
	if (w > 560.0f && measuredHz > 1.0f) {
		wchar_t hzBuf[32];
		swprintf_s(hzBuf, L"%.0f Hz", measuredHz);
		renderer.DrawText(L"Pen", w - 420, y, 30, h, Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignRight);
		D2D1_COLOR_F hzCol = (measuredHz > 200) ? Theme::Success() : (measuredHz > 50) ? Theme::AccentPrimary() : Theme::Warning();
		renderer.DrawText(hzBuf, w - 385, y, 60, h, hzCol, renderer.pFontSmall, Renderer::AlignLeft);
	}
}

//
// === Live Cursor Dot on tablet area preview ===
//
void AetherApp::DrawLiveCursor(float previewX, float previewY, float previewW, float previewH, float fullW, float fullH) {
	if (!driver.penActive.load()) return;

	float px = driver.penX.load();
	float py = driver.penY.load();
	float pressure = driver.penPressure.load();

	// Map pen mm position to the full tablet preview
	// previewX/Y represent the full tablet surface (0..fullW, 0..fullH)
	float dotX = previewX + (px / fullW) * previewW;
	float dotY = previewY + (py / fullH) * previewH;

	// Clamp to preview bounds
	dotX = Clamp(dotX, previewX, previewX + previewW);
	dotY = Clamp(dotY, previewY, previewY + previewH);

	// Animate
	liveCursorPulseT += deltaTime * 3.0f;
	float pulse = 1.0f + 0.15f * sinf(liveCursorPulseT);

	// Outer glow
	D2D1_COLOR_F glow = Theme::AccentPrimary();
	glow.a = 0.15f + 0.1f * sinf(liveCursorPulseT);
	renderer.FillCircle(dotX, dotY, 8.0f * pulse, glow);

	// Ring
	D2D1_COLOR_F ring = Theme::AccentSecondary();
	ring.a = 0.5f;
	renderer.DrawCircle(dotX, dotY, 5.0f * pulse, ring, 1.0f);

	// Inner dot — size based on pressure
	float dotR = 2.0f + pressure * 2.0f;
	renderer.FillCircle(dotX, dotY, dotR, Theme::AccentPrimary());

	// White center
	renderer.FillCircle(dotX, dotY, 1.2f, D2D1::ColorF(0xFFFFFF, 0.9f));
}

//
// === Input Trail Visualizer ===
//
void AetherApp::DrawInputVisualizer(float x, float y, float w, float h) {
	std::vector<DriverBridge::TrailPoint> points;
	{
		std::lock_guard<std::mutex> lock(driver.trailMutex);
		points = driver.trail;
	}

	if (points.size() < 2) return;

	// Use actual tablet dimensions reported by service
	float fullW = (driver.tabletWidth > 1.0f) ? driver.tabletWidth : 152.0f;
	float fullH = (driver.tabletHeight > 1.0f) ? driver.tabletHeight : 95.0f;

	// Filter out expired points (age > 2.5s) and limit to last 200
	int count = (int)points.size();
	int start = (count > 200) ? count - 200 : 0;

	// Find first non-expired point
	while (start < count && points[start].age > 2.5f) start++;

	int visibleCount = count - start;
	if (visibleCount < 2) return;

	for (int i = start + 1; i < count; i++) {
		// Skip expired segments
		if (points[i].age > 2.5f || points[i - 1].age > 2.5f) continue;

		float t = (float)(i - start) / (float)(visibleCount); // 0..1 old to new

		// Fade based on both position in trail and age
		float ageFade = 1.0f - Clamp(points[i].age / 2.5f, 0.0f, 1.0f);

		// Map mm to preview coordinates
		float x1 = x + (points[i - 1].x / fullW) * w;
		float y1 = y + (points[i - 1].y / fullH) * h;
		float x2 = x + (points[i].x / fullW) * w;
		float y2 = y + (points[i].y / fullH) * h;

		// Clamp to preview bounds
		x1 = Clamp(x1, x, x + w); y1 = Clamp(y1, y, y + h);
		x2 = Clamp(x2, x, x + w); y2 = Clamp(y2, y, y + h);

		// Fade older points
		float alpha = t * 0.6f * ageFade;
		D2D1_COLOR_F col = Theme::AccentPrimary();
		col.a = alpha;

		float lineW = (0.5f + t * 1.5f) * ageFade;
		renderer.DrawLine(x1, y1, x2, y2, col, lineW);
	}

	// Draw head dot (only if recent)
	if (count > 0 && points[count - 1].age < 0.5f) {
		const auto& last = points[count - 1];
		float hx = x + (last.x / fullW) * w;
		float hy = y + (last.y / fullH) * h;
		renderer.FillCircle(hx, hy, 2.5f, Theme::AccentSecondary());
	}
}

//
// === Theme slot color helpers ===
//
void AetherApp::GetThemeSlotColor(Theme::ThemeData& t, int slot, float& r, float& g, float& b) {
	switch (slot) {
	case 0: r = t.bgDeep[0]; g = t.bgDeep[1]; b = t.bgDeep[2]; break;
	case 1: r = t.bgBase[0]; g = t.bgBase[1]; b = t.bgBase[2]; break;
	case 2: r = t.bgSurface[0]; g = t.bgSurface[1]; b = t.bgSurface[2]; break;
	case 3: r = t.bgElevated[0]; g = t.bgElevated[1]; b = t.bgElevated[2]; break;
	case 4: r = t.textPri[0]; g = t.textPri[1]; b = t.textPri[2]; break;
	case 5: r = t.accent[0]; g = t.accent[1]; b = t.accent[2]; break;
	}
}

void AetherApp::SetThemeSlotColor(Theme::ThemeData& t, int slot, float r, float g, float b) {
	switch (slot) {
	case 0: t.bgDeep[0]=r; t.bgDeep[1]=g; t.bgDeep[2]=b; break;
	case 1: t.bgBase[0]=r; t.bgBase[1]=g; t.bgBase[2]=b; break;
	case 2: t.bgSurface[0]=r; t.bgSurface[1]=g; t.bgSurface[2]=b; break;
	case 3: t.bgElevated[0]=r; t.bgElevated[1]=g; t.bgElevated[2]=b; break;
	case 4: t.textPri[0]=r; t.textPri[1]=g; t.textPri[2]=b;
		// Also update secondary/muted proportionally
		t.textSec[0]=r*0.71f; t.textSec[1]=g*0.71f; t.textSec[2]=b*0.71f;
		t.textMut[0]=r*0.48f; t.textMut[1]=g*0.48f; t.textMut[2]=b*0.48f;
		break;
	case 5: t.accent[0]=r; t.accent[1]=g; t.accent[2]=b; break;
	}
}

void AetherApp::ResetThemeToDefault(int themeIndex) {
	if (themeIndex >= 0 && themeIndex < uiThemeCount && uiThemeDefaults[themeIndex]) {
		uiThemes[themeIndex] = *uiThemeDefaults[themeIndex];
		if (themeIndex == currentTheme) {
			Theme::ApplyTheme(uiThemes[themeIndex]);
			accentPicker.SetRGB(uiThemes[themeIndex].accent[0], uiThemes[themeIndex].accent[1], uiThemes[themeIndex].accent[2]);
			if (hWnd) { extern void ApplyAetherWindowTheme(HWND); ApplyAetherWindowTheme(hWnd); }
		}
	}
}

//
// === Full UI Theme Selector ===
//
void AetherApp::DrawThemeSelector(float x, float& y, float w) {
	int cols = 4;
	float cardW = (w - (cols - 1) * 8.0f) / (float)cols;
	float cardH = 64.0f;
	float gap = 8.0f;

	for (int i = 0; i < uiThemeCount; i++) {
		int col = i % cols;
		int row = i / cols;
		float cx = x + col * (cardW + gap);
		float cy = y + row * (cardH + gap);

		Theme::ThemeData& t = uiThemes[i];

		bool hovered = PointInRect(mouseX, mouseY, cx, cy, cardW, cardH);
		themeHoverT[i] = Lerp(themeHoverT[i], hovered ? 1.0f : 0.0f, deltaTime * Theme::Anim::SpeedFast);
		bool isActive = (i == currentTheme);

		// Card background
		D2D1_COLOR_F cardBg = D2D1::ColorF(t.bgSurface[0], t.bgSurface[1], t.bgSurface[2]);
		renderer.FillRoundedRect(cx, cy, cardW, cardH, 8, cardBg);

		// Color strip at top
		D2D1_COLOR_F accentCol = D2D1::ColorF(t.accent[0], t.accent[1], t.accent[2]);
		D2D1_COLOR_F bgCol = D2D1::ColorF(t.bgBase[0], t.bgBase[1], t.bgBase[2]);
		renderer.FillRoundedRect(cx + 4, cy + 4, cardW - 8, 12, 4, bgCol);
		renderer.FillRoundedRect(cx + 4, cy + 4, (cardW - 8) * 0.5f, 12, 4, accentCol);

		// Theme name
		D2D1_COLOR_F nameCol = D2D1::ColorF(t.textPri[0], t.textPri[1], t.textPri[2]);
		renderer.DrawText(t.name, cx + 8, cy + 22, cardW - 40, 20, nameCol, renderer.pFontSmall, Renderer::AlignLeft);

		// === Clickable color dots (6 slots) ===
		float dotY = cy + 46;
		float dotR = 5.0f;
		float dotGap = 13.0f;
		float dotStartX = cx + 10;

		for (int s = 0; s < THEME_SLOT_COUNT; s++) {
			float dCX = dotStartX + s * dotGap;
			float dCY = dotY;
			float sr, sg, sb;
			GetThemeSlotColor(t, s, sr, sg, sb);

			// Draw dot
			D2D1_COLOR_F dotCol = D2D1::ColorF(sr, sg, sb);
			renderer.FillCircle(dCX, dCY, dotR, dotCol);
			renderer.DrawCircle(dCX, dCY, dotR, D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.3f), 0.5f);

			// Hover highlight
			bool dotHovered = PointInRect(mouseX, mouseY, dCX - dotR, dCY - dotR, dotR * 2, dotR * 2);
			if (dotHovered) {
				renderer.DrawCircle(dCX, dCY, dotR + 2, Theme::AccentPrimary(), 1.5f);
				Tooltip::Show(mouseX, mouseY, themeSlotNames[s], deltaTime);
			}

			// Click to edit this slot
			if (dotHovered && mouseClicked) {
				// If not already active, switch to this theme first
				if (!isActive) {
					currentTheme = i;
					Theme::ApplyTheme(t);
					accentPicker.SetRGB(t.accent[0], t.accent[1], t.accent[2]);
					if (hWnd) { extern void ApplyAetherWindowTheme(HWND); ApplyAetherWindowTheme(hWnd); }
				}
				// Open picker for this slot
				editingTheme = i;
				editingSlot = s;
				slotPicker.SetRGB(sr, sg, sb);
			}
		}

		// Active indicator
		if (isActive) {
			renderer.DrawRoundedRect(cx, cy, cardW, cardH, 8, Theme::AccentPrimary(), 2.0f);
			// Checkmark badge
			float badgeR = 10.0f;
			float badgeCX = cx + cardW - badgeR - 4;
			float badgeCY = cy + badgeR + 4;
			renderer.FillCircle(badgeCX, badgeCY, badgeR, Theme::AccentPrimary());
			renderer.DrawCircle(badgeCX, badgeCY, badgeR, D2D1::ColorF(0xFFFFFF, 0.3f), 1.0f);
			renderer.DrawText(L"\x2713", badgeCX - 7, badgeCY - 7, 14, 14, D2D1::ColorF(0xFFFFFF), renderer.pFontSmall, Renderer::AlignCenter);

			// Reset button (small) — right side of name
			float resetX = cx + cardW - 38;
			float resetY = cy + 24;
			float resetW = 32, resetH = 16;
			bool resetHovered = PointInRect(mouseX, mouseY, resetX, resetY, resetW, resetH);
			D2D1_COLOR_F resetBg = resetHovered ? Theme::BgHover() : D2D1::ColorF(0, 0, 0, 0);
			if (resetHovered) {
				renderer.FillRoundedRect(resetX, resetY, resetW, resetH, 3, resetBg);
				Tooltip::Show(mouseX, mouseY, L"Reset this theme to default colors", deltaTime);
			}
			renderer.DrawText(L"Reset", resetX, resetY, resetW, resetH, 
				resetHovered ? Theme::TextPrimary() : Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignCenter);
			if (resetHovered && mouseClicked) {
				ResetThemeToDefault(i);
				editingTheme = -1;
				editingSlot = -1;
				AutoSaveConfig();
			}
		} else {
			D2D1_COLOR_F border = LerpColor(D2D1::ColorF(t.borderSub[0], t.borderSub[1], t.borderSub[2], t.borderSub[3]),
				accentCol, themeHoverT[i] * 0.5f);
			renderer.DrawRoundedRect(cx, cy, cardW, cardH, 8, border);
		}

		// Click on card body (not dots, not reset) to apply theme
		if (hovered && mouseClicked && !isActive) {
			// Check not clicking a dot
			bool clickedDot = false;
			for (int s = 0; s < THEME_SLOT_COUNT; s++) {
				float dCX = dotStartX + s * dotGap;
				if (PointInRect(mouseX, mouseY, dCX - dotR, dotY - dotR, dotR * 2, dotR * 2)) {
					clickedDot = true; break;
				}
			}
			if (!clickedDot) {
				currentTheme = i;
				Theme::ApplyTheme(t);
				accentPicker.SetRGB(t.accent[0], t.accent[1], t.accent[2]);
				wchar_t hexBuf[16];
				swprintf_s(hexBuf, L"#%02X%02X%02X", (int)(t.accent[0] * 255), (int)(t.accent[1] * 255), (int)(t.accent[2] * 255));
				wcscpy_s(hexColorInput.buffer, hexBuf);
				hexColorInput.cursor = (int)wcslen(hexBuf);
				if (hWnd) { extern void ApplyAetherWindowTheme(HWND); ApplyAetherWindowTheme(hWnd); }
				editingTheme = -1;
				editingSlot = -1;
				AutoSaveConfig();
			}
		}
	}

	int rows = (uiThemeCount + cols - 1) / cols;
	y += rows * (cardH + gap) + 8.0f;

	// === Inline color picker for editing a slot ===
	if (editingTheme >= 0 && editingTheme < uiThemeCount && editingSlot >= 0) {
		Theme::ThemeData& et = uiThemes[editingTheme];

		// Background panel
		float pickerPanelW = w;
		float pickerPanelH = slotPicker.GetTotalHeight() + 72;
		renderer.FillRoundedRect(x, y, pickerPanelW, pickerPanelH, 8, Theme::BgElevated());
		renderer.DrawRoundedRect(x, y, pickerPanelW, pickerPanelH, 8, Theme::BorderAccent());

		// Label: "Editing [ThemeName] > [SlotName]"
		wchar_t editLabel[128];
		swprintf_s(editLabel, L"Editing: %s \x2022 %s", et.name, themeSlotNames[editingSlot]);
		renderer.DrawText(editLabel, x + 12, y + 6, pickerPanelW - 80, 20, Theme::TextAccent(), renderer.pFontSmall);

		// Close button
		{
			float closeX = x + pickerPanelW - 60, closeY = y + 6, closeW = 48, closeH = 20;
			bool closeHovered = PointInRect(mouseX, mouseY, closeX, closeY, closeW, closeH);
			if (closeHovered) renderer.FillRoundedRect(closeX, closeY, closeW, closeH, 4, Theme::BgHover());
			renderer.DrawText(L"Close", closeX, closeY, closeW, closeH,
				closeHovered ? Theme::TextPrimary() : Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignCenter);
			if (closeHovered && mouseClicked) {
				editingTheme = -1; editingSlot = -1;
			}
		}

		// Color picker
		slotPicker.x = x + 12;
		slotPicker.y = y + 30;
		slotPicker.width = pickerPanelW * 0.45f;
		if (slotPicker.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) {
			float r, g, b;
			slotPicker.GetRGB(r, g, b);
			SetThemeSlotColor(et, editingSlot, r, g, b);
			// Apply live
			if (editingTheme == currentTheme) {
				Theme::ApplyTheme(et);
				if (editingSlot == 5) {
					accentPicker.SetRGB(r, g, b);
					wchar_t hexBuf[16];
					swprintf_s(hexBuf, L"#%02X%02X%02X", (int)(r * 255), (int)(g * 255), (int)(b * 255));
					wcscpy_s(hexColorInput.buffer, hexBuf);
					hexColorInput.cursor = (int)wcslen(hexBuf);
				}
				if (hWnd) { extern void ApplyAetherWindowTheme(HWND); ApplyAetherWindowTheme(hWnd); }
			}
			AutoSaveConfig();
		}
		slotPicker.Draw(renderer);

		// Hex input below picker for manual color entry
		{
			float hexLabelY = slotPicker.y + slotPicker.GetTotalHeight() + 4;
			renderer.DrawText(L"Hex:", x + 12, hexLabelY, 30, 24, Theme::TextMuted(), renderer.pFontSmall);
			slotHexInput.x = x + 44;
			slotHexInput.y = hexLabelY;
			slotHexInput.width = slotPicker.width - 34;
			slotHexInput.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
			slotHexInput.Draw(renderer);

			// Sync: when not focused, show current picker color
			if (!slotHexInput.focused) {
				float pr, pg, pb;
				slotPicker.GetRGB(pr, pg, pb);
				wchar_t hb[16];
				swprintf_s(hb, L"#%02X%02X%02X", (int)(pr*255), (int)(pg*255), (int)(pb*255));
				wcscpy_s(slotHexInput.buffer, hb);
				slotHexInput.cursor = (int)wcslen(hb);
			}
		}

		// Show all slot buttons on the right for quick switching
		float slotBtnX = x + pickerPanelW * 0.5f + 20;
		float slotBtnY = y + 32;
		for (int s = 0; s < THEME_SLOT_COUNT; s++) {
			float sr, sg, sb;
			GetThemeSlotColor(et, s, sr, sg, sb);
			float bx = slotBtnX, by = slotBtnY + s * 26.0f;
			float bw = pickerPanelW * 0.45f - 20, bh = 22.0f;

			bool slotActive = (s == editingSlot);
			bool slotHov = PointInRect(mouseX, mouseY, bx, by, bw, bh);

			D2D1_COLOR_F slotBg = slotActive ? Theme::AccentDim() : (slotHov ? Theme::BgHover() : D2D1::ColorF(0, 0, 0, 0));
			renderer.FillRoundedRect(bx, by, bw, bh, 4, slotBg);

			// Color swatch
			renderer.FillCircle(bx + 10, by + bh * 0.5f, 5, D2D1::ColorF(sr, sg, sb));
			renderer.DrawCircle(bx + 10, by + bh * 0.5f, 5, Theme::BorderSubtle(), 0.5f);

			// Label
			D2D1_COLOR_F lblCol = slotActive ? Theme::AccentPrimary() : (slotHov ? Theme::TextPrimary() : Theme::TextSecondary());
			renderer.DrawText(themeSlotNames[s], bx + 22, by, bw - 22, bh, lblCol, renderer.pFontSmall);

			// Click to switch slot
			if (slotHov && mouseClicked) {
				editingSlot = s;
				slotPicker.SetRGB(sr, sg, sb);
			}
		}

		y += pickerPanelH + 8.0f;
	}
}

//
// === Profile Selector ===
//
void AetherApp::DrawProfileSelector(float x, float y, float w) {
	float btnW = 80.0f;
	float btnH = 26.0f;
	float gap = 6.0f;

	for (int i = 0; i < MAX_PROFILES; i++) {
		float bx = x + i * (btnW + gap);
		profileBtns[i].x = bx;
		profileBtns[i].y = y;
		profileBtns[i].width = btnW;
		profileBtns[i].height = btnH;
		profileBtns[i].isPrimary = (i == currentProfile);

		if (profileBtns[i].Update(mouseX, mouseY, mouseClicked, deltaTime)) {
			if (i == currentProfile) {
				// Save to current profile
				wchar_t path[MAX_PATH];
				GetModuleFileNameW(nullptr, path, MAX_PATH);
				std::wstring dir(path);
				size_t slash = dir.find_last_of(L"\\/");
				if (slash != std::wstring::npos) dir = dir.substr(0, slash + 1);
				std::wstring profilePath = dir + L"aether_profile_" + std::to_wstring(i) + L".cfg";
				SaveConfig(profilePath);
			} else {
				// Switch to profile
				currentProfile = i;
				wchar_t path[MAX_PATH];
				GetModuleFileNameW(nullptr, path, MAX_PATH);
				std::wstring dir(path);
				size_t slash = dir.find_last_of(L"\\/");
				if (slash != std::wstring::npos) dir = dir.substr(0, slash + 1);
				std::wstring profilePath = dir + L"aether_profile_" + std::to_wstring(i) + L".cfg";
				DWORD attrs = GetFileAttributesW(profilePath.c_str());
				if (attrs != INVALID_FILE_ATTRIBUTES) {
					LoadConfig(profilePath);
					ApplyAllSettings();
				}
			}
		}
		profileBtns[i].Draw(renderer);
	}
}

//
// === Hz Meter ===
// Reads the real report rate measured by AetherService
//
void AetherApp::UpdateHzMeter() {
	if (!driver.penActive.load()) {
		measuredHz = 0;
		return;
	}
	float serviceHz = driver.penHz.load();
	if (serviceHz > 0.1f) {
		// Smooth the displayed value to avoid flickering
		measuredHz = Lerp(measuredHz, serviceHz, deltaTime * 3.0f);
	}
}
