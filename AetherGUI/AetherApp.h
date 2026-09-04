#pragma once
#include "Framework.h"
#include "Renderer.h"
#include "Controls.h"
#include "DriverBridge.h"

#ifndef WM_AETHER_UPDATE_AVAILABLE
#define WM_AETHER_UPDATE_AVAILABLE (WM_APP + 42)
#endif

struct PendingUpdateInfo {
	std::wstring latestTag;
	std::wstring currentVersion;
	std::wstring releaseUrl;
};

class AetherApp {
public:
	HWND hWnd = nullptr;
	Renderer renderer;
	DriverBridge driver;

	float mouseX = 0, mouseY = 0;
	bool mouseDown = false;
	bool mouseClicked = false;
	float scrollDelta = 0;

	bool middleMouseDown = false;
	bool rightMouseDown = false;
	bool isDragScrolling = false;
	float dragScrollStartY = 0;
	float dragScrollStartOffset = 0;

	bool isDraggingArea = false;
	int dragTarget = 0;
	float dragStartMouseX = 0, dragStartMouseY = 0;
	float dragStartValX = 0, dragStartValY = 0;
	float dragScale = 1.0f;

	float detectedScreenW = 1920;
	float detectedScreenH = 1080;
	float primaryScreenW = 1920;
	float primaryScreenH = 1080;
	float virtualScreenX = 0;
	float virtualScreenY = 0;
	float clientWidth = Theme::Size::WindowWidth;
	float clientHeight = Theme::Size::WindowHeight;

	struct DisplayTarget {
		float x = 0;
		float y = 0;
		float width = 0;
		float height = 0;
		std::wstring label;
	};
	std::vector<DisplayTarget> displayTargets;
	int selectedDisplayTarget = 0;

	std::chrono::high_resolution_clock::time_point lastFrameTime;
	float deltaTime = 0.016f;

	TabBar sidebar;
	int currentTab = 0;

	float tabTransitionT = 1.0f;
	int prevTab = 0;
	float tabSlideOffset = 0.0f;
	float tabFadeAlpha = 1.0f;

	float aboutAnimT = 0.0f;
	float bgAnimT = 0.0f;
	struct BgOrb {
		float x, y;
		float vx, vy;
		float radius;
		float phase;
		float alpha;
	};
	BgOrb bgOrbs[7];
	bool bgOrbsInitialized = false;
	struct ShootingStar {
		float x, y;
		float vx, vy;
		float life;
		float maxLife;
		float tailLen;
		float brightness;
		bool active;
	};
	static const int MAX_STARS = 6;
	ShootingStar stars[MAX_STARS];
	float starSpawnTimer = 0.0f;
	struct TwinkleStar {
		float x, y;
		float phase;
		float speed;
		float size;
		float alpha;
	};
	static const int MAX_TWINKLE_STARS = 70;
	TwinkleStar twinkleStars[MAX_TWINKLE_STARS];
	bool twinkleStarsInitialized = false;

	int particleStyle = 0;
	int accentAnimMode = 0;
	int animSpeedMode = 0;
	std::wstring bgImagePath;
	ID2D1Bitmap* bgImageBitmap = nullptr;
	bool bgImageLoaded = false;
	bool bgImageFailed = false;
	wchar_t bgImageErrorText[96] = L"";
	std::thread bgReadThread;
	std::vector<unsigned char> bgPixels;
	std::atomic<bool> bgReadDone{ false };
	bool bgReadOk = false;
	UINT bgPixelW = 0, bgPixelH = 0;
	UINT bgBitmapGen = 0;
	UINT bgDecodeMax = 0;
	bool bgRetriedPath = false;
	bool bgReadWasCache = false;
	std::chrono::steady_clock::time_point lastActivityTime{};
	float accentAnimT = 0.0f;
	float uiAccentBaseR = 0.498f, uiAccentBaseG = 0.608f, uiAccentBaseB = 0.831f;
	struct Firefly {
		float x, y;
		float baseX, baseY;
		float phase, speed;
		float radius, alpha;
		float wanderAngle;
		bool active;
	};
	static const int MAX_FIREFLIES = 20;
	Firefly fireflies[MAX_FIREFLIES];
	bool firefliesInitialized = false;

	struct Snowflake {
		float x, y;
		float speed, drift;
		float size, alpha;
		float wobblePhase;
		bool active;
	};
	static const int MAX_SNOWFLAKES = 40;
	Snowflake snowflakes[MAX_SNOWFLAKES];
	bool snowInitialized = false;

	struct {
		Slider tabletWidth, tabletHeight, tabletX, tabletY;
		Slider screenWidth, screenHeight, screenX, screenY;
		Slider rotation;
		Slider aspectRatio;
		Toggle customValues;
		Toggle autoCenter;
		Toggle lockAspect;
	} area;

	struct {
		Toggle smoothingEnabled;
		Slider smoothingLatency;
		Slider smoothingInterval;

		Toggle antichatterEnabled;
		Slider antichatterStrength;
		Slider antichatterMultiplier;

		Toggle noiseEnabled;
		Slider noiseBuffer;
		Slider noiseThreshold;
		Slider noiseIterations;

		Toggle reconstructorEnabled;
		Slider reconStrength;
		Slider reconSmooth;
		Slider reconReverseEma;
		Slider reconPrediction;

		Toggle jitterStabEnabled;
		Slider jitterStabRadius;
		Slider jitterStabRelease;

		Toggle lazyMouseEnabled;
		Slider lazyMouseRadius;
		Slider lazyMouseSmooth;

		Toggle temporalEnabled;
		Slider temporalPrediction;
		Slider temporalSmoothing;
		Slider temporalReverseEma;
		Slider temporalFollow;

		Toggle predictionEnabled;
		Slider predictionStrength;
		Slider predictionSharpness;

		Toggle pressureCurveEnabled;
		Slider pressureExponent;
		Slider pressureMin;
		Slider pressureMax;

	} filters;

	RadioGroup outputMode;
	Slider dpiScale;
	Slider bgImageOpacity;

	CycleSelector buttonTip;
	CycleSelector buttonBottom;
	CycleSelector buttonTop;
	int openButtonDropdown = -1;
	int capturingButtonMap = -1;
	float buttonDropdownY = 0.0f;
	float buttonDropdownBoxX = 0.0f;
	float buttonDropdownBoxW = 0.0f;

	Toggle forceFullArea;
	Toggle areaClipping;
	Toggle areaLimiting;
	Slider tipThreshold;
	Toggle overclockEnabled;
	Slider overclockHz;
	Toggle penRateLimitEnabled;
	Slider penRateLimitHz;

	RadioGroup rateMode;

	Button saveConfigBtn;
	Button loadConfigBtn;
	Button updateOpenBtn;
	Button updateLaterBtn;
	Button runtimeMissingOpenBtn;
	Button runtimeMissingLaterBtn;
	Button vmultiMissingOpenBtn;
	Button vmultiMissingLaterBtn;
	Button doctorScanBtn;
	Button doctorJitterBtn;
	Button installPluginBtn;
	Button installLuaPluginBtn;
	Button bgImageBtn;
	Button bgClearBtn;
	Button installSourcePluginBtn;
	Button reloadPluginBtn;
	Button listPluginBtn;
	Button pluginManagerInstallBtn;
	Button pluginManagerInstallSourceBtn;
	Button pluginManagerRefreshBtn;
	Button pluginManagerDeleteBtn;
	Button pluginManagerCloseBtn;
	Button pluginManagerSourceBtn;
	Button pluginManagerSourceCodeBtn;
	Button pluginManagerWikiBtn;
	Button pluginManagerApplySourceBtn;
	Button pluginManagerCancelSourceBtn;
	Button pluginManagerInstallRepoBtn;
	TextInput pluginRepoOwnerInput;
	TextInput pluginRepoNameInput;
	TextInput pluginRepoRefInput;
	struct PluginCatalogEntry {
		std::wstring name;
		std::wstring owner;
		std::wstring description;
		std::wstring version;
		std::wstring driverVersion;
		std::wstring downloadUrl;
		std::wstring repositoryUrl;
		std::wstring wikiUrl;
		std::wstring license;
		std::string sourcePath;
		bool nativeAvailable = false;
		bool sourcePort = false;
		bool installed = false;
		bool needsUpdate = false;
		std::wstring installedIdentity;
	};
	struct PluginEntry {
		struct PluginOption {
			enum Kind {
				SliderOption,
				ToggleOption
			};
			Kind kind = SliderOption;
			std::string key;
			std::wstring label;
			Slider slider;
			Toggle toggle;
		};
		std::wstring key;
		std::wstring name;
		std::wstring dllName;
		std::wstring description;
		Toggle enabled;
		std::vector<PluginOption> options;
	};
	std::vector<PluginEntry> pluginEntries;
	std::vector<PluginCatalogEntry> pluginCatalogEntries;
	std::wstring pluginListStatus = L"Plugin list not scanned";
	std::wstring pluginCatalogStatus = L"Repository not loaded";
	std::wstring pluginRepoOwner;
	std::wstring pluginRepoName = L"AetherGUI";
	std::wstring pluginRepoRef = L"main";
	bool pluginListDirty = true;
	bool pluginManagerOpen = false;
	bool pluginSourceEditorOpen = false;
	bool updateModalOpen = false;
	std::wstring updateLatestTag;
	std::wstring updateCurrentVersion;
	std::wstring updateReleaseUrl;
	bool runtimeMissingModalOpen = false;
	std::wstring missingRuntimeNames;
	bool vmultiMissingModalOpen = false;
	std::wstring vmultiMissingModeName;
	bool CheckVCRedist();
	void DrawRuntimeMissingModal();
	void DrawVMultiMissingModal();
	void CheckVMultiModeSelected();
	float pluginCatalogScrollY = 0.0f;
	float pluginCatalogDragStartY = 0.0f;
	float pluginCatalogDragStartOffset = 0.0f;
	bool isPluginCatalogDragScrolling = false;
	int pluginManagerTab = 0;
	int selectedPluginIndex = 0;
	int selectedCatalogIndex = 0;
	float pluginCatalogAnimT = 1.0f;
	int pluginCatalogAnimDirection = 1;
	bool autoStartEnabled = true;
	float autoStartRetryTimer = 0.0f;
	int autoStartRetryCount = 0;
	float pendingStartupSettingsTimer = -1.0f;
	struct ConfigEntry {
		std::wstring name;
		std::wstring path;
		bool autoSave = true;
	};
	std::vector<ConfigEntry> configEntries;
	std::wstring activeConfigPath;
	int selectedConfigIndex = -1;

	bool loadingFromFile = false;

	bool suppressNamedAutosave = false;

	bool pendingTabletInfoResync = false;
	float lastSeenTabletWidth  = -1.0f;
	float lastSeenTabletHeight = -1.0f;

	float consoleScrollY = 0;
	TextInput consoleInput;
	std::vector<std::string> commandHistory;
	int commandHistoryIdx = 0;

	Button startStopBtn;
	Button autoStartBtn;
	std::wstring servicePath;
	bool autoStartAttempted = false;
	bool vmultiInstalled = false;
	bool vmultiCheckDone = false;

	struct SettingsSnapshot {
		std::vector<float> sliders;
		std::vector<int> ints;
		std::vector<bool> toggles;
	};
	std::vector<SettingsSnapshot> undoStack;
	SettingsSnapshot lastSettingsSnapshot;
	bool hasSettingsSnapshot = false;
	bool applyingUndo = false;

	void SaveConfig(const std::wstring& path);
	void LoadConfig(const std::wstring& path);

	float areaScrollY = 0;
	float filterScrollY = 0;
	float settingsScrollY = 0;
	float brushScrollY = 0;
	float doctorScrollY = 0;
	float areaContentH = 0;
	float filterContentH = 0;
	float settingsContentH = 0;
	float brushContentH = 0;
	float doctorContentH = 0;
	std::vector<std::wstring> doctorConflicts;
	bool doctorScanned = false;
	bool doctorJitterRunning = false;
	float doctorJitterElapsed = 0.0f;
	std::vector<float> doctorJitterX;
	std::vector<float> doctorJitterY;
	float doctorJitterAvg = -1.0f;
	float doctorJitterMax = -1.0f;
	int doctorJitterCount = 0;
	bool doctorJitterMoved = false;
	float doctorJitterJumpFrac = 0.0f;
	std::vector<float> doctorPressureHistory;
	float doctorPeakPressure = 0.0f;

	Button displayPrevBtn;
	Button displayNextBtn;

	ColorPicker accentPicker;

	struct {
		Toggle enabled;
		Toggle stabilizerEnabled;
		Slider stabilizerStability;
		Slider stabilizerSensitivity;
		Toggle snappingEnabled;
		Slider snappingInner;
		Slider snappingOuter;
	} aether;

	struct {
		Toggle enabled;
		Slider holdMs;
	} clickStabilize;

	static const int MAX_THEMES = 14;
	const Theme::ThemeData* uiThemeDefaults[MAX_THEMES];
	Theme::ThemeData uiThemes[MAX_THEMES];
	int uiThemeCount = 0;
	int currentTheme = 0;
	float themeHoverT[MAX_THEMES] = {};

	TextInput hexColorInput;

	int editingTheme = -1;
	int editingSlot = -1;
	ColorPicker slotPicker;
	TextInput slotHexInput;

	static const int THEME_SLOT_COUNT = 6;
	const wchar_t* themeSlotNames[THEME_SLOT_COUNT] = {
		L"Deep BG", L"Base BG", L"Surface", L"Elevated", L"Text", L"Accent"
	};

	void GetThemeSlotColor(Theme::ThemeData& t, int slot, float& r, float& g, float& b);
	void SetThemeSlotColor(Theme::ThemeData& t, int slot, float r, float g, float b);
	void ResetThemeToDefault(int themeIndex);

	float liveCursorAnimT = 0;
	float liveCursorPulseT = 0;

	bool showVisualizer = false;
	Toggle visualizerToggle;
	float vizAnimT = 0;

	float measuredHz = 0;

	bool Initialize(HWND hwnd);

	void Shutdown();

	void Tick();

	void OnMouseMove(float x, float y);

	void OnMouseDown();

	void OnMouseUp();

	void OnMouseWheel(float delta);

	void OnMiddleMouseDown();

	void OnMiddleMouseUp();

	void OnRightMouseDown();

	void OnRightMouseUp();

	void OnXMouseDown(int xbutton);

	void OnChar(wchar_t ch);

	void OnKeyDown(int vk);

	void OnResize(UINT width, UINT height);

	void OnDisplayChange();

private:

	void InitControls();

	void UpdateControls();

	void SendFilterSettings();

	void ApplyAllSettings();

	void CaptureSettingsSnapshot(SettingsSnapshot& snapshot) const;

	void ApplySettingsSnapshot(const SettingsSnapshot& snapshot);

	bool SettingsSnapshotsEqual(const SettingsSnapshot& a, const SettingsSnapshot& b) const;

	void TrackSettingsUndo();

	void PushUndoCheckpoint();

	void InitializeSettingsUndo();

	void UndoLastSettingsChange();

	bool IsTextEditingActive() const;

	bool FocusNextEditableRow(bool reverse);

	void SwitchTabByKeyboard(int direction);

	float GetSystemDpiScale() const;

	float GetSelectedDpiScale() const;

	void ApplyDpiScale();

	void AutoSaveConfig();

	void AutoLoadConfig();

	std::wstring GetLastLoadedMarkerPath();
	void SaveLastLoadedMarker();
	std::wstring ReadLastLoadedMarker();

	std::wstring GetAutosaveFlagsPath();
	bool IsConfigAutosaveEnabled(const std::wstring& configPath);
	void LoadAutosaveFlags();
	void SaveAutosaveFlags();

	void PrepareModalDialog();

	void SyncLoadedControlVisuals();

	std::wstring GetConfigPath();

	std::wstring GetConfigDirectory();

	void EnsureConfigDirectory();

	void RefreshConfigFiles();

	bool SaveConfigWithDialog();

	bool LoadConfigWithDialog();

public:

	bool LoadConfigByHotkey(int oneBasedIndex);

	struct HotkeyBinding {
		UINT mods;
		UINT vk;
	};
	static const int kConfigHotkeyCount = 9;
	HotkeyBinding configHotkeys[kConfigHotkeyCount];

	int capturingHotkeySlot = -1;

	struct ActionHotkey {
		int actionIndex;
		UINT mods;
		UINT vk;
	};
	static const int kActionHotkeyCount = 8;
	ActionHotkey actionHotkeys[kActionHotkeyCount];
	int capturingActionHotkey = -1;
	int openActionDropdown = -1;
	float actionDropdownY = 0.0f;
	float actionDropdownX = 0.0f;
	float actionDropdownW = 0.0f;
#if defined(_WIN32)
	HHOOK actionHotkeyHook = NULL;
#endif
	void InstallActionHotkeyHook();
	void UninstallActionHotkeyHook();
	void FireActionHotkey(int slot);

	void SendActionHotkeysToDriver();
	std::wstring ActionHotkeyLabel(int slot) const;

	void RegisterConfigHotkeys();
	void UnregisterConfigHotkeys();
	std::wstring HotkeyLabelForSlot(int oneBasedIndex) const;
	void ResetHotkeyDefaults();

	struct AppProfileEntry {
		std::wstring processName;
		std::wstring configPath;
	};
	std::vector<AppProfileEntry> appProfiles;
	bool   appProfilesEnabled = false;
	std::wstring lastForegroundProcess;
	std::wstring lastForeignForeground;
	DWORD  lastForegroundPollTick = 0;

	void PollForegroundProcess();
	std::wstring GetForegroundProcessName();
	void AddAppProfileFromForeground();
	void SaveAppProfiles();
	void LoadAppProfiles();
	std::wstring GetAppProfilesPath();

	bool   calibrationOpen = false;
	int    calibrationStep = 0;
	bool   calibrationWaitingForLift = false;
	float  calibrationCapturedX[4]{};
	float  calibrationCapturedY[4]{};
	float  calibrationLastSeenX = 0;
	float  calibrationLastSeenY = 0;
	bool   calibrationLastSeenValid = false;

	void StartCalibration();
	void TickCalibration();
	void UpdateAccentAnimation();
	void DrawSegmentedRow(const wchar_t* const* names, int count, int* value, float x, float y, float w);
	void ApplyAnimationSpeedMode();
	bool ChooseBackgroundImage();
	std::wstring GetBgCachePath();
	void KickBgRead(const std::wstring& path);
	bool NeedsFullFrameRate() const;
	void DrawCalibrationModal();
	void ApplyCalibrationResult();

private:

	bool InstallPluginWithDialog();
	bool InstallLuaPluginWithDialog();

	bool InstallPluginSourceWithDialog();

	void RefreshPluginList();

	void ConfigurePluginDefaults(PluginEntry& entry);

	void SendPluginSettings();

	void SendPluginEnable(size_t pluginIndex);

	void SendPluginOptions(size_t pluginIndex);

	void SendPluginOption(size_t pluginIndex, const PluginEntry::PluginOption& option);

	bool RefreshPluginCatalog();

	void UpdatePluginCatalogInstallState();

	void ClampPluginCatalogScroll();

	void DrawPluginFilterControls(float cx, float& y, float cw, float hw, float filterRightX, bool filterSingleColumn, bool& filterChanged);

	void DrawPluginManagerModal();

	void DrawPluginSourceModal();

	void DrawUpdateModal();

	bool DeleteInstalledPlugin(size_t index);

	std::wstring GetPluginDirectory() const;

	bool InstallRepositoryPlugin(PluginCatalogEntry& entry);

	bool DownloadGitHubSourcePort(const PluginCatalogEntry& entry, std::wstring& folderPath, std::wstring& status);

	void OpenExternalUrl(const std::wstring& url);

	void DrawConfigManager(float x, float& y, float w);

	void DrawHeader();

	void DrawBackground();

	void DrawLogoBadge(float x, float y);

	void DrawAreaPanel();

	void DrawFilterPanel();

	void DrawConsolePanel();

	void DrawSettingsPanel();

	void DrawAboutPanel();
	void DrawDoctorPanel();
	void DoctorScanConflicts();
	void DoctorKillProcess(const std::wstring& name);

	void DrawBrushPanel();

	void DrawStatusBar();

	void DrawButtonDropdownOverlay();

	void DrawActionHotkeyDropdownOverlay();

	void DrawLiveCursor(float previewX, float previewY, float previewW, float previewH, float fullW, float fullH);

	void DrawInputVisualizer(float x, float y, float w, float h);

	void DrawThemeSelector(float x, float& y, float w);

	void UpdateHzMeter();

	void DrawOverclockInfo(float x, float y, float w);

	void RefreshDetectedScreen();

	void ClampScrollOffsets();

	void SendDisplaySettingsToDriver();

	void SendStaticMonitorInfoToDriver();

public:
	bool StartDriverService();

	void toggleAutoStart();
	bool isAutoStartEnabled() const;

	void ShowUpdateModal(const std::wstring& latestTag, const std::wstring& currentVersion, const std::wstring& releaseUrl);

private:

	void SendStartupSettingsToDriver();

	void ApplyDisplayTarget(int index);

	float GetScreenAspectRatio() const;

	void ClampScreenArea();

	void CenterScreenArea();

	void ClampTabletAreaToFull(float fullTabletW, float fullTabletH);

	void ApplyAspectLock(bool preserveHeight);

	float GetContentAreaTop();

	float GetContentAreaBottom();

	void BeginClipContent();

	void EndClipContent();
};
