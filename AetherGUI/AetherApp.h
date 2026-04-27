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

	// Drag-scroll with middle/right mouse button
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

	// Particle style: 0=Shooting Stars, 1=Fireflies, 2=Aurora, 3=Snow, 4=None
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

	// === Aether Smooth filter controls ===
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

	// === Full UI Themes ===
	static const int MAX_THEMES = 14;
	const Theme::ThemeData* uiThemeDefaults[MAX_THEMES]; // originals for reset
	Theme::ThemeData uiThemes[MAX_THEMES];               // editable runtime copies
	int uiThemeCount = 0;
	int currentTheme = 0;
	float themeHoverT[MAX_THEMES] = {};

	// === Hex color input ===
	TextInput hexColorInput;

	// === Theme inline color editor ===
	int editingTheme = -1;    // which theme card has picker open (-1 = none)
	int editingSlot = -1;     // which color slot (0-5)
	ColorPicker slotPicker;
	TextInput slotHexInput;   // hex input for editing slot color

	static const int THEME_SLOT_COUNT = 6;
	const wchar_t* themeSlotNames[THEME_SLOT_COUNT] = {
		L"Deep BG", L"Base BG", L"Surface", L"Elevated", L"Text", L"Accent"
	};

	// Get/Set color slot from ThemeData
	void GetThemeSlotColor(Theme::ThemeData& t, int slot, float& r, float& g, float& b);
	void SetThemeSlotColor(Theme::ThemeData& t, int slot, float r, float g, float b);
	void ResetThemeToDefault(int themeIndex);

	// === Profile system ===
	int currentProfile = 0;
	static const int MAX_PROFILES = 4;
	struct ProfileSlot {
		std::wstring name;
		std::wstring path;
		bool exists;
	};
	ProfileSlot profiles[MAX_PROFILES];
	Button profileBtns[MAX_PROFILES];

	// === Live cursor dot ===
	float liveCursorAnimT = 0;
	float liveCursorPulseT = 0;

	// === Input Visualizer ===
	bool showVisualizer = false;
	Toggle visualizerToggle;
	float vizAnimT = 0;

	// === Hz meter (reads from service) ===
	float measuredHz = 0;

	/// Initialize renderer, controls and auto-start the driver service
	bool Initialize(HWND hwnd);
	/// Release all resources and stop the driver
	void Shutdown();
	/// Main loop frame: update input, draw UI, present
	void Tick();

	/// Track cursor position for hover/drag interactions
	void OnMouseMove(float x, float y);
	/// Begin mouse press — starts drag or click
	void OnMouseDown();
	/// End mouse press — finalize drag and apply settings
	void OnMouseUp();
	/// Scroll content panels or sliders
	void OnMouseWheel(float delta);
	/// Begin drag-scroll with middle mouse button
	void OnMiddleMouseDown();
	/// End drag-scroll with middle mouse button
	void OnMiddleMouseUp();
	/// Begin drag-scroll with right mouse button
	void OnRightMouseDown();
	/// End drag-scroll with right mouse button
	void OnRightMouseUp();
	/// Route keyboard input to active text fields and sliders
	void OnChar(wchar_t ch);
	/// Handle special keys (arrows, clipboard, command history)
	void OnKeyDown(int vk);
	/// Resize render target and recalculate layout
	void OnResize(UINT width, UINT height);
	/// React to monitor configuration changes
	void OnDisplayChange();

private:
	/// Create all UI controls with default values and layout
	void InitControls();
	/// Reserved for future per-frame control updates
	void UpdateControls();
	/// Push current filter configuration to the driver service
	void SendFilterSettings();
	/// Push all settings (area, filters, buttons, overclock) to the driver
	void ApplyAllSettings();
	/// Save current config to the default path next to the executable
	void AutoSaveConfig();
	/// Load config from the default path if it exists
	void AutoLoadConfig();
	/// Return the full path to aether_config.cfg next to the executable
	std::wstring GetConfigPath();

	/// Draw the top header bar with logo, status pill and start/stop button
	void DrawHeader();
	/// Draw the animated background (stars, dot grid)
	void DrawBackground();
	/// Draw the Aether logo image or fallback glyph
	void DrawLogoBadge(float x, float y);
	/// Draw the Area tab: screen map, tablet area, options, output mode
	void DrawAreaPanel();
	/// Draw the Filters tab: smoothing, antichatter, noise, snap, etc.
	void DrawFilterPanel();
	/// Draw the Console tab: driver log output and command input
	void DrawConsolePanel();
	/// Draw the Settings tab: button mapping, pen settings, overclock
	void DrawSettingsPanel();
	/// Draw the About tab: logo animation, credits, feature list
	void DrawAboutPanel();
	/// Draw the bottom status bar with connection info, mode, and Hz meter
	void DrawStatusBar();
	/// Draw live cursor dot on tablet area preview
	void DrawLiveCursor(float previewX, float previewY, float previewW, float previewH, float fullW, float fullH);
	/// Draw the input trail visualizer overlay
	void DrawInputVisualizer(float x, float y, float w, float h);
	/// Draw full UI theme selector cards
	void DrawThemeSelector(float x, float& y, float w);
	/// Draw profile selector buttons
	void DrawProfileSelector(float x, float y, float w);
	/// Update Hz meter from pen position timestamps
	void UpdateHzMeter();
	/// Draw the overclock info card with Hz meter
	void DrawOverclockInfo(float x, float y, float w);
	/// Re-enumerate monitors and update display target list
	void RefreshDetectedScreen();
	/// Ensure scroll offsets stay within content bounds
	void ClampScrollOffsets();
	/// Send desktop size and screen area to the driver
	void SendDisplaySettingsToDriver();
	/// Launch AetherService.exe and send initial settings
	bool StartDriverService();
	/// Send all startup settings (area, mode, buttons, filters) then start
	void SendStartupSettingsToDriver();
	/// Switch to a different display target and apply its resolution
	void ApplyDisplayTarget(int index);
	/// Calculate screen width/height ratio for aspect lock
	float GetScreenAspectRatio() const;
	/// Clamp screen area values to detected desktop bounds
	void ClampScreenArea();
	/// Clamp tablet area within full tablet dimensions
	void ClampTabletAreaToFull(float fullTabletW, float fullTabletH);
	/// Enforce aspect ratio lock on tablet area dimensions
	void ApplyAspectLock(bool preserveHeight);

	/// Get Y coordinate where scrollable content begins
	float GetContentAreaTop();
	/// Get Y coordinate where scrollable content ends (above status bar)
	float GetContentAreaBottom();
	/// Push a clip rectangle covering the content area
	void BeginClipContent();
	/// Pop the content area clip rectangle
	void EndClipContent();
};
