#pragma once
#include "Framework.h"
#include "Renderer.h"
#include "Controls.h"
#include "DriverBridge.h"

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

	
	int particleStyle = 0;
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
		Toggle customValues;
		Toggle lockAspect;
	} area;

	struct {
		Toggle smoothingEnabled;
		Slider smoothingLatency;
		Slider smoothingInterval;

		Toggle antichatterEnabled;
		Slider antichatterStrength;
		Slider antichatterMultiplier;
		Slider antichatterOffsetX;
		Slider antichatterOffsetY;

		Toggle noiseEnabled;
		Slider noiseBuffer;
		Slider noiseThreshold;
		Slider noiseIterations;

		Toggle velCurveEnabled;
		Slider velCurveMinSpeed;
		Slider velCurveMaxSpeed;
		Slider velCurveSmoothing;
		Slider velCurveSharpness;

		Toggle snapEnabled;
		Slider snapRadius;
		Slider snapSmooth;

		Toggle reconstructorEnabled;
		Slider reconStrength;
		Slider reconVelSmooth;
		Slider reconAccelCap;
		Slider reconPredTime;

		Toggle adaptiveEnabled;
		Slider adaptiveProcessNoise;
		Slider adaptiveMeasNoise;
		Slider adaptiveVelWeight;
	} filters;

	RadioGroup outputMode;

	CycleSelector buttonTip;
	CycleSelector buttonBottom;
	CycleSelector buttonTop;

	Toggle forceFullArea;
	Toggle areaClipping;
	Toggle areaLimiting;
	Slider tipThreshold;
	Toggle overclockEnabled;
	Slider overclockHz;

	Button saveConfigBtn;
	Button loadConfigBtn;

	float consoleScrollY = 0;
	TextInput consoleInput;
	std::vector<std::string> commandHistory;
	int commandHistoryIdx = 0;

	Button startStopBtn;
	std::string servicePath;
	bool autoStartAttempted = false;
	bool vmultiInstalled = false;
	bool vmultiCheckDone = false;

	void SaveConfig(const std::wstring& path);
	void LoadConfig(const std::wstring& path);

	float areaScrollY = 0;
	float filterScrollY = 0;
	float settingsScrollY = 0;
	float areaContentH = 0;
	float filterContentH = 0;
	float settingsContentH = 0;

	Button displayPrevBtn;
	Button displayNextBtn;

	ColorPicker accentPicker;

	
	struct {
		Toggle enabled;
		Toggle lagRemovalEnabled;
		Slider lagRemovalStrength;
		Toggle stabilizerEnabled;
		Slider stabilizerStability;
		Slider stabilizerSensitivity;
		Toggle snappingEnabled;
		Slider snappingInner;
		Slider snappingOuter;
		Toggle suppressionEnabled;
		Slider suppressionTime;
	} aether;

	
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

	
	int currentProfile = 0;
	static const int MAX_PROFILES = 4;
	struct ProfileSlot {
		std::wstring name;
		std::wstring path;
		bool exists;
	};
	ProfileSlot profiles[MAX_PROFILES];
	Button profileBtns[MAX_PROFILES];

	
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
	
	void OnChar(wchar_t ch);
	
	void OnKeyDown(int vk);
	
	void OnResize(UINT width, UINT height);
	
	void OnDisplayChange();

private:
	
	void InitControls();
	
	void UpdateControls();
	
	void SendFilterSettings();
	
	void ApplyAllSettings();
	
	void AutoSaveConfig();
	
	void AutoLoadConfig();
	
	std::wstring GetConfigPath();

	
	void DrawHeader();
	
	void DrawBackground();
	
	void DrawLogoBadge(float x, float y);
	
	void DrawAreaPanel();
	
	void DrawFilterPanel();
	
	void DrawConsolePanel();
	
	void DrawSettingsPanel();
	
	void DrawAboutPanel();
	
	void DrawStatusBar();
	
	void DrawLiveCursor(float previewX, float previewY, float previewW, float previewH, float fullW, float fullH);
	
	void DrawInputVisualizer(float x, float y, float w, float h);
	
	void DrawThemeSelector(float x, float& y, float w);
	
	void DrawProfileSelector(float x, float y, float w);
	
	void UpdateHzMeter();
	
	void DrawOverclockInfo(float x, float y, float w);
	
	void RefreshDetectedScreen();
	
	void ClampScrollOffsets();
	
	void SendDisplaySettingsToDriver();
	
	bool StartDriverService();
	
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
