#include "Framework.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#include "AetherApp.h"
#include "resource.h"


static HICON CreateIconFromPNG(const std::wstring& pngPath, int size) {
	IWICImagingFactory* wicFactory = nullptr;
	IWICBitmapDecoder* decoder = nullptr;
	IWICBitmapFrameDecode* frame = nullptr;
	IWICBitmapScaler* scaler = nullptr;
	IWICFormatConverter* converter = nullptr;
	HICON hIcon = nullptr;

	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
	if (FAILED(hr)) goto cleanup;

	hr = wicFactory->CreateDecoderFromFilename(pngPath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
	if (FAILED(hr)) goto cleanup;

	hr = decoder->GetFrame(0, &frame);
	if (FAILED(hr)) goto cleanup;

	
	hr = wicFactory->CreateBitmapScaler(&scaler);
	if (FAILED(hr)) goto cleanup;

	hr = scaler->Initialize(frame, size, size, WICBitmapInterpolationModeHighQualityCubic);
	if (FAILED(hr)) goto cleanup;

	
	hr = wicFactory->CreateFormatConverter(&converter);
	if (FAILED(hr)) goto cleanup;

	hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppBGRA,
		WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
	if (FAILED(hr)) goto cleanup;

	{
		UINT stride = size * 4;
		UINT bufSize = stride * size;
		std::vector<BYTE> colorBits(bufSize, 0);
		std::vector<BYTE> maskBits((size * size + 7) / 8, 0); 

		hr = converter->CopyPixels(nullptr, stride, bufSize, colorBits.data());
		if (FAILED(hr)) goto cleanup;

		
		BITMAPINFO bmi = {};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = size;
		bmi.bmiHeader.biHeight = -(LONG)size; 
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		HDC hdc = GetDC(nullptr);
		void* dibBits = nullptr;
		HBITMAP hbmColor = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
		if (hbmColor && dibBits) {
			memcpy(dibBits, colorBits.data(), bufSize);

			
			BYTE* px = (BYTE*)dibBits;
			for (UINT i = 0; i < (UINT)(size * size); i++) {
				BYTE a = px[3];
				px[0] = (BYTE)((px[0] * a) / 255);
				px[1] = (BYTE)((px[1] * a) / 255);
				px[2] = (BYTE)((px[2] * a) / 255);
				px += 4;
			}
		}

		HBITMAP hbmMask = CreateBitmap(size, size, 1, 1, maskBits.data());

		if (hbmColor && hbmMask) {
			ICONINFO ii = {};
			ii.fIcon = TRUE;
			ii.hbmColor = hbmColor;
			ii.hbmMask = hbmMask;
			hIcon = CreateIconIndirect(&ii);
		}

		if (hbmColor) DeleteObject(hbmColor);
		if (hbmMask) DeleteObject(hbmMask);
		ReleaseDC(nullptr, hdc);
	}

cleanup:
	if (converter) converter->Release();
	if (scaler) scaler->Release();
	if (frame) frame->Release();
	if (decoder) decoder->Release();
	if (wicFactory) wicFactory->Release();
	return hIcon;
}


static std::wstring FindLogoPNG() {
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring dir(exePath);
	size_t slash = dir.find_last_of(L'\\');
	if (slash != std::wstring::npos) dir = dir.substr(0, slash + 1);

	std::wstring candidates[] = {
		dir + L"aether_logo.png",
		dir + L"assets\\aether_logo.png",
		dir + L"..\\aether_logo.png",
		dir + L"..\\assets\\aether_logo.png",
		dir + L"..\\..\\AetherGUI\\assets\\aether_logo.png",
		dir + L"..\\..\\assets\\aether_logo.png",
		dir + L"..\\..\\..\\AetherGUI\\assets\\aether_logo.png",
		dir + L"..\\..\\..\\assets\\aether_logo.png"
	};
	for (const auto& c : candidates) {
		DWORD attrs = GetFileAttributesW(c.c_str());
		if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
			return c;
	}
	return L"";
}

AetherApp app;
bool isRunning = true;


#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_SHOW     4001
#define ID_TRAY_START    4002
#define ID_TRAY_STOP     4003
#define ID_TRAY_PLUGINS  4004
#define ID_TRAY_CONSOLE  4005
#define ID_TRAY_EXIT     4006
NOTIFYICONDATAW nid = {};
bool trayCreated = false;
bool windowHidden = false;

void CreateTrayIcon(HWND hWnd, HINSTANCE hInstance) {
	nid.cbSize = sizeof(nid);
	nid.hWnd = hWnd;
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	
	int traySize = GetSystemMetrics(SM_CXSMICON);
	{
		std::wstring png = FindLogoPNG();
		if (!png.empty())
			nid.hIcon = CreateIconFromPNG(png, traySize);
	}
	if (!nid.hIcon)
		nid.hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_AETHER_ICON),
			IMAGE_ICON, traySize, traySize, LR_DEFAULTCOLOR);
	if (!nid.hIcon) nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
	wcscpy_s(nid.szTip, L"Aether Tablet Driver");
	Shell_NotifyIconW(NIM_ADD, &nid);
	trayCreated = true;
}

void RemoveTrayIcon() {
	if (trayCreated) {
		Shell_NotifyIconW(NIM_DELETE, &nid);
		trayCreated = false;
	}
}

void MinimizeToTray(HWND hWnd) {
	ShowWindow(hWnd, SW_HIDE);
	windowHidden = true;
}

void RestoreFromTray(HWND hWnd) {
	ShowWindow(hWnd, SW_SHOW);
	ShowWindow(hWnd, SW_RESTORE);
	SetForegroundWindow(hWnd);
	windowHidden = false;
}

void ShowTrayMenu(HWND hWnd) {
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();

	MENUITEMINFOW title = {};
	title.cbSize = sizeof(title);
	title.fMask = MIIM_STRING | MIIM_STATE;
	title.fState = MFS_DISABLED;
	title.dwTypeData = const_cast<LPWSTR>(L"Aether Tablet Driver");
	InsertMenuItemW(hMenu, 0, TRUE, &title);
	AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

	AppendMenuW(hMenu, MF_STRING, ID_TRAY_SHOW, windowHidden ? L"Open Aether" : L"Show Aether");
	AppendMenuW(hMenu, MF_STRING, app.driver.isConnected ? ID_TRAY_STOP : ID_TRAY_START,
		app.driver.isConnected ? L"Stop driver" : L"Start driver");
	AppendMenuW(hMenu, MF_STRING, ID_TRAY_PLUGINS, L"Plugins");
	AppendMenuW(hMenu, MF_STRING, ID_TRAY_CONSOLE, L"Console");
	AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit Aether");
	
	SetForegroundWindow(hWnd);
	TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
	DestroyMenu(hMenu);
}

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#ifndef DWMWA_COLOR_NONE
#define DWMWA_COLOR_NONE 0xFFFFFFFE
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

void EnablePerMonitorDpiAwareness() {
	HMODULE user32 = GetModuleHandleW(L"user32.dll");
	if (user32) {
		auto setDpiAwarenessContext = reinterpret_cast<BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT)>(
			GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
		if (setDpiAwarenessContext &&
			setDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
			return;
		}
	}
	SetProcessDPIAware();
}

void ApplyAetherWindowTheme(HWND hWnd) {
	
	BOOL darkMode = Theme::IsLightTheme() ? FALSE : TRUE;
	DwmSetWindowAttribute(hWnd, 19, &darkMode, sizeof(darkMode));
	DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

	COLORREF caption = RGB(
		(int)(Theme::Base::BgSurfR * 255),
		(int)(Theme::Base::BgSurfG * 255),
		(int)(Theme::Base::BgSurfB * 255));
	COLORREF border = RGB(
		(int)(Theme::Base::BgBaseR * 255),
		(int)(Theme::Base::BgBaseG * 255),
		(int)(Theme::Base::BgBaseB * 255));
	COLORREF text = RGB(
		(int)(Theme::Base::TextPriR * 255),
		(int)(Theme::Base::TextPriG * 255),
		(int)(Theme::Base::TextPriB * 255));
	DwmSetWindowAttribute(hWnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
	DwmSetWindowAttribute(hWnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
	DwmSetWindowAttribute(hWnd, DWMWA_TEXT_COLOR, &text, sizeof(text));

	DWM_WINDOW_CORNER_PREFERENCE corner = static_cast<DWM_WINDOW_CORNER_PREFERENCE>(DWMWCP_ROUND);
	DwmSetWindowAttribute(hWnd, 33, &corner, sizeof(corner));

	
	SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME);
}

RECT GetNearestWorkArea(HWND hWnd) {
	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	if (monitor && GetMonitorInfoW(monitor, &mi))
		return mi.rcWork;

	RECT work = {};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
	return work;
}

void FitWindowToWorkArea(HWND hWnd) {
	RECT windowRect = {};
	GetWindowRect(hWnd, &windowRect);
	RECT work = GetNearestWorkArea(hWnd);

	int width = windowRect.right - windowRect.left;
	int height = windowRect.bottom - windowRect.top;
	int workWidth = work.right - work.left;
	int workHeight = work.bottom - work.top;

	width = std::min(width, workWidth);
	height = std::min(height, workHeight);

	int x = windowRect.left;
	int y = windowRect.top;
	if (x + width > work.right) x = work.right - width;
	if (y + height > work.bottom) y = work.bottom - height;
	if (x < work.left) x = work.left;
	if (y < work.top) y = work.top;

	SetWindowPos(hWnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_MOUSEMOVE:
		app.OnMouseMove((float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam));
		return 0;

	case WM_LBUTTONDOWN:
		SetCapture(hWnd);
		app.OnMouseDown();
		return 0;

	case WM_LBUTTONUP:
		ReleaseCapture();
		app.OnMouseUp();
		return 0;

	case WM_MBUTTONDOWN:
		SetCapture(hWnd);
		app.OnMiddleMouseDown();
		return 0;

	case WM_MBUTTONUP:
		ReleaseCapture();
		app.OnMiddleMouseUp();
		return 0;

	case WM_RBUTTONDOWN:
		SetCapture(hWnd);
		app.OnRightMouseDown();
		return 0;

	case WM_RBUTTONUP:
		ReleaseCapture();
		app.OnRightMouseUp();
		return 0;

	case WM_MOUSEWHEEL:
		app.OnMouseWheel((float)GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f);
		return 0;

	case WM_CHAR:
		app.OnChar((wchar_t)wParam);
		return 0;

	case WM_KEYDOWN:
		app.OnKeyDown((int)wParam);
		return 0;

	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED) {
			MinimizeToTray(hWnd);
			return 0;
		}
		app.OnResize(LOWORD(lParam), HIWORD(lParam));
		if (app.renderer.pRT) app.Tick();
		return 0;

	case WM_TRAYICON:
		if (lParam == WM_LBUTTONDBLCLK || lParam == WM_LBUTTONUP) {
			RestoreFromTray(hWnd);
		}
		else if (lParam == WM_RBUTTONUP) {
			ShowTrayMenu(hWnd);
		}
		return 0;

	case WM_COMMAND:
		if (LOWORD(wParam) == ID_TRAY_SHOW) {
			RestoreFromTray(hWnd);
		}
		else if (LOWORD(wParam) == ID_TRAY_START) {
			app.autoStartEnabled = true;
			app.autoStartRetryTimer = 0.0f;
			app.StartDriverService();
		}
		else if (LOWORD(wParam) == ID_TRAY_STOP) {
			app.autoStartEnabled = false;
			app.driver.Stop();
		}
		else if (LOWORD(wParam) == ID_TRAY_PLUGINS) {
			RestoreFromTray(hWnd);
			app.sidebar.activeIndex = 1;
			app.pluginManagerOpen = true;
		}
		else if (LOWORD(wParam) == ID_TRAY_CONSOLE) {
			RestoreFromTray(hWnd);
			app.sidebar.activeIndex = 3;
		}
		else if (LOWORD(wParam) == ID_TRAY_EXIT) {
			RemoveTrayIcon();
			DestroyWindow(hWnd);
		}
		return 0;

	case WM_AETHER_UPDATE_AVAILABLE: {
		PendingUpdateInfo* info = reinterpret_cast<PendingUpdateInfo*>(lParam);
		if (info) {
			app.ShowUpdateModal(info->latestTag, info->currentVersion, info->releaseUrl);
			delete info;
			if (windowHidden)
				RestoreFromTray(hWnd);
		}
		return 0;
	}

	case WM_CLOSE:
		MinimizeToTray(hWnd);
		return 0;

	case WM_DISPLAYCHANGE:
		app.OnDisplayChange();
		FitWindowToWorkArea(hWnd);
		InvalidateRect(hWnd, nullptr, FALSE);
		return 0;

	case WM_DPICHANGED:
		if (lParam) {
			RECT* suggested = reinterpret_cast<RECT*>(lParam);
			SetWindowPos(hWnd, nullptr,
				suggested->left, suggested->top,
				suggested->right - suggested->left,
				suggested->bottom - suggested->top,
				SWP_NOZORDER | SWP_NOACTIVATE);
		}
		ApplyAetherWindowTheme(hWnd);
		return 0;

	case WM_DWMCOMPOSITIONCHANGED:
		ApplyAetherWindowTheme(hWnd);
		return DefWindowProcW(hWnd, message, wParam, lParam);

	case WM_ACTIVATE:
	case WM_SETTINGCHANGE:
		
		
		return DefWindowProcW(hWnd, message, wParam, lParam);

	case WM_DESTROY:
		RemoveTrayIcon();
		isRunning = false;
		PostQuitMessage(0);
		return 0;

	case WM_GETMINMAXINFO: {
		MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
		mmi->ptMinTrackSize.x = 640;
		mmi->ptMinTrackSize.y = 480;
		return 0;
	}

	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT: {
		PAINTSTRUCT ps;
		BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		return 0;
	}

	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
	
	HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"AetherGUI_SingleInstance");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		
		HWND existing = FindWindowW(L"AetherDriverClass", nullptr);
		if (existing) {
			if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
			ShowWindow(existing, SW_SHOW);
			SetForegroundWindow(existing);
		}
		if (hMutex) CloseHandle(hMutex);
		return 0;
	}

	EnablePerMonitorDpiAwareness();

	
	{
		struct { ULONG Version; ULONG ControlMask; ULONG StateMask; } throttling = {};
		throttling.Version = 1;
		throttling.ControlMask = 0x1;
		throttling.StateMask = 0;
		SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &throttling, sizeof(throttling));
	}
	timeBeginPeriod(1);

	
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	static HBRUSH hDarkBrush = CreateSolidBrush(RGB(0x09, 0x09, 0x0B));
	wc.hbrBackground = hDarkBrush;
	wc.lpszClassName = L"AetherDriverClass";

	
	
	std::wstring logoPng = FindLogoPNG();
	HICON hIconClassBig = nullptr;
	HICON hIconClassSmall = nullptr;
	if (!logoPng.empty()) {
		OutputDebugStringW((L"[Aether] Found logo PNG: " + logoPng + L"\n").c_str());
		hIconClassBig = CreateIconFromPNG(logoPng, GetSystemMetrics(SM_CXICON));
		hIconClassSmall = CreateIconFromPNG(logoPng, GetSystemMetrics(SM_CXSMICON));
		if (hIconClassBig) OutputDebugStringW(L"[Aether] Created big icon from PNG OK\n");
		else OutputDebugStringW(L"[Aether] FAILED to create big icon from PNG\n");
	} else {
		OutputDebugStringW(L"[Aether] Logo PNG not found, falling back to .ico resource\n");
	}
	if (!hIconClassBig) hIconClassBig = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_AETHER_ICON),
		IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	if (!hIconClassBig) hIconClassBig = LoadIconW(nullptr, IDI_APPLICATION);
	if (!hIconClassSmall) hIconClassSmall = hIconClassBig;
	wc.hIcon = hIconClassBig;
	wc.hIconSm = hIconClassSmall;
	RegisterClassExW(&wc);

	int winW = (int)Theme::Size::WindowWidth;
	int winH = (int)Theme::Size::WindowHeight;

	RECT wr = { 0, 0, winW, winH };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
	int adjW = wr.right - wr.left;
	int adjH = wr.bottom - wr.top;

	RECT work = {};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
	int workW = work.right - work.left;
	int workH = work.bottom - work.top;
	adjW = std::min(adjW, workW);
	adjH = std::min(adjH, workH);
	int startX = work.left + (workW - adjW) / 2;
	int startY = work.top + (workH - adjH) / 2;

	HWND hWnd = CreateWindowExW(
		0,
		L"AetherDriverClass",
		L"Aether - Tablet Driver",
		WS_OVERLAPPEDWINDOW,
		startX, startY,
		adjW, adjH,
		nullptr, nullptr, hInstance, nullptr
	);

	if (!hWnd) return 1;

	
	HICON hIconBig = nullptr;
	HICON hIconSmall = nullptr;
	if (!logoPng.empty()) {
		
		int bigSize = std::max(48, GetSystemMetrics(SM_CXICON));
		hIconBig = CreateIconFromPNG(logoPng, bigSize);
		hIconSmall = CreateIconFromPNG(logoPng, GetSystemMetrics(SM_CXSMICON));
	}
	if (!hIconBig) hIconBig = hIconClassBig;
	if (!hIconSmall) hIconSmall = hIconClassSmall;
	if (hIconBig) SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
	if (hIconSmall) SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

	ApplyAetherWindowTheme(hWnd);
	FitWindowToWorkArea(hWnd);
	CreateTrayIcon(hWnd, hInstance);

	if (!app.Initialize(hWnd)) {
		MessageBoxW(hWnd, L"Failed to initialize Direct2D renderer.", L"Aether Error", MB_ICONERROR);
		return 1;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	MSG msg = {};
	auto nextFrameTime = std::chrono::steady_clock::now();
	while (isRunning) {
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				isRunning = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		if (isRunning) {
			if (windowHidden) {
				Sleep(50);
				nextFrameTime = std::chrono::steady_clock::now();
				continue;
			}

			auto now = std::chrono::steady_clock::now();
			if (now >= nextFrameTime) {
				app.Tick();
				nextFrameTime = now + std::chrono::milliseconds(16);
			}
			else {
				auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(nextFrameTime - now).count();
				Sleep((DWORD)Clamp((float)remaining, 1.0f, 8.0f));
			}
		}
	}

	app.Shutdown();
	if (hMutex) {
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
	}
	return 0;
}
