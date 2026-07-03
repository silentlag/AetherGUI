#include "stdafx.h"
#include "TabletFilterPlugin.h"
#include "AetherPluginManager.h"

#define LOG_MODULE "Plugin"
#include "Logger.h"

#if defined(_WIN32)
	#define AETHER_TRY        __try
	#define AETHER_CATCH_FAIL __except (EXCEPTION_EXECUTE_HANDLER)
#else
	#define AETHER_TRY        try
	#define AETHER_CATCH_FAIL catch (...)
#endif

static void* SafePluginCreate(AetherPluginCreateFn fn) {
	if (fn == NULL)
		return NULL;
	AETHER_TRY {
		return fn();
	}
	AETHER_CATCH_FAIL {
		return NULL;
	}
}

static void SafePluginDestroy(AetherPluginDestroyFn fn, void* instance, bool* crashed) {
	if (crashed) *crashed = false;
	if (fn == NULL || instance == NULL)
		return;
	AETHER_TRY {
		fn(instance);
	}
	AETHER_CATCH_FAIL {
		if (crashed) *crashed = true;
	}
}

static bool SafePluginReset(AetherPluginResetFn fn, void* instance, const AetherPluginPoint* point) {
	if (fn == NULL)
		return true;
	AETHER_TRY {
		fn(instance, point);
		return true;
	}
	AETHER_CATCH_FAIL {
		return false;
	}
}

static bool SafePluginSetDouble(AetherPluginSetDoubleFn fn, void* instance, const char* key, double value, int* result) {
	if (result) *result = 0;
	if (fn == NULL)
		return true;
	AETHER_TRY {
		if (result) *result = fn(instance, key, value);
		return true;
	}
	AETHER_CATCH_FAIL {
		return false;
	}
}

static bool SafePluginSetString(AetherPluginSetStringFn fn, void* instance, const char* key, const char* value, int* result) {
	if (result) *result = 0;
	if (fn == NULL)
		return true;
	AETHER_TRY {
		if (result) *result = fn(instance, key, value);
		return true;
	}
	AETHER_CATCH_FAIL {
		return false;
	}
}

static bool SafePluginProcess(AetherPluginProcessFn fn, void* instance, AetherPluginPoint* point) {
	if (fn == NULL)
		return true;
	AETHER_TRY {
		fn(instance, point);
		return true;
	}
	AETHER_CATCH_FAIL {
		return false;
	}
}

TabletFilterPlugin::TabletFilterPlugin() {
	module = platform::DynLib{};
	instance = NULL;
	destroyFn = NULL;
	resetFn = NULL;
	processFn = NULL;
	setDoubleFn = NULL;
	setStringFn = NULL;
	z = 0;
	buttons = 0;
	pressure = 0;
	hoverDistance = 0;
	firstUpdate = true;
	lastTime = std::chrono::high_resolution_clock::now();
}

TabletFilterPlugin::~TabletFilterPlugin() {
	Unload();
}

bool TabletFilterPlugin::Load(const std::wstring& dllPath) {
	Unload();

	if (!PluginAllowlistCheck(dllPath)) {
		LOG_ERROR("Plugin rejected by allowlist: %ls\n", dllPath.c_str());
		return false;
	}

	module = platform::DynLibLoad(dllPath.c_str());
	if (!platform::DynLibValid(module)) {
		LOG_ERROR("Failed to load plugin: %ls (%s)\n",
			dllPath.c_str(), platform::DynLibLastError().c_str());
		return false;
	}

	AetherPluginGetInfoFn getInfoFn = (AetherPluginGetInfoFn)platform::DynLibSymbol(module, "AetherPluginGetInfo");
	AetherPluginCreateFn createFn = (AetherPluginCreateFn)platform::DynLibSymbol(module, "AetherPluginCreate");
	destroyFn = (AetherPluginDestroyFn)platform::DynLibSymbol(module, "AetherPluginDestroy");
	resetFn = (AetherPluginResetFn)platform::DynLibSymbol(module, "AetherPluginReset");
	processFn = (AetherPluginProcessFn)platform::DynLibSymbol(module, "AetherPluginProcess");
	setDoubleFn = (AetherPluginSetDoubleFn)platform::DynLibSymbol(module, "AetherPluginSetDouble");
	setStringFn = (AetherPluginSetStringFn)platform::DynLibSymbol(module, "AetherPluginSetString");

	if (getInfoFn == NULL || processFn == NULL) {
		LOG_ERROR("Plugin missing required exports: %ls\n", dllPath.c_str());
		Unload();
		return false;
	}

	AetherPluginInfo info = {};
	if (!getInfoFn(&info) || info.apiVersion != AETHER_PLUGIN_API_VERSION) {
		LOG_ERROR("Plugin API version mismatch: %ls\n", dllPath.c_str());
		Unload();
		return false;
	}

	path = dllPath;
	name = info.name != NULL && info.name[0] ? info.name : "Unnamed plugin";
	description = info.description != NULL ? info.description : "";

	instance = SafePluginCreate(createFn);
	if (createFn != NULL && instance == NULL) {
		LOG_ERROR("Plugin create crashed: %ls\n", dllPath.c_str());
		Unload();
		return false;
	}

	isEnabled = true;
	isValid = true;
	firstUpdate = true;
	lastTime = std::chrono::high_resolution_clock::now();

	LOG_INFO("Loaded plugin: %s\n", name.c_str());
	return true;
}

void TabletFilterPlugin::Unload() {
	if (platform::DynLibValid(module)) {
		bool destroyCrashed = false;
		SafePluginDestroy(destroyFn, instance, &destroyCrashed);
		if (destroyCrashed)
			LOG_ERROR("Plugin destroy crashed: %ls\n", path.c_str());

		platform::DynLibUnload(module);
	}

	instance = NULL;
	destroyFn = NULL;
	resetFn = NULL;
	processFn = NULL;
	setDoubleFn = NULL;
	setStringFn = NULL;
	isEnabled = false;
	isValid = false;
}

void TabletFilterPlugin::SetTarget(Vector2D vector, double h) {
	target.Set(vector);
	z = h;
}

void TabletFilterPlugin::SetPosition(Vector2D vector, double h) {
	position.Set(vector);
	z = h;
}

void TabletFilterPlugin::SetReportState(BYTE buttons, double pressure, double hoverDistance) {
	this->buttons = buttons;
	this->pressure = pressure;
	this->hoverDistance = hoverDistance;
}

bool TabletFilterPlugin::GetPosition(Vector2D *outputVector) {
	outputVector->Set(position);
	return true;
}

void TabletFilterPlugin::Reset(Vector2D pos) {
	std::lock_guard<std::mutex> lock(pluginMutex);
	position.Set(pos);
	target.Set(pos);
	firstUpdate = true;
	lastTime = std::chrono::high_resolution_clock::now();

	if (resetFn != NULL) {
		AetherPluginPoint point = {};
		point.x = pos.x;
		point.y = pos.y;
		point.z = z;
		point.dt = 0;
		point.isValid = 1;
		point.buttons = buttons;
		point.tipDown = (buttons & 0x01) ? 1 : 0;
		point.pressure = pressure;
		point.hoverDistance = hoverDistance;
		point.tiltX = 0;
		point.tiltY = 0;
		if (!SafePluginReset(resetFn, instance, &point)) {
			LOG_ERROR("Plugin reset crashed: %s\n", name.c_str());
			isEnabled = false;
			isValid = false;
		}
	}
}

bool TabletFilterPlugin::SetDoubleOption(const std::string& key, double value) {
	std::lock_guard<std::mutex> lock(pluginMutex);
	if (setDoubleFn == NULL)
		return false;
	int result = 0;
	if (!SafePluginSetDouble(setDoubleFn, instance, key.c_str(), value, &result)) {
		LOG_ERROR("Plugin option crashed: %s.%s\n", name.c_str(), key.c_str());
		isEnabled = false;
		isValid = false;
		return false;
	}
	return result != 0;
}

bool TabletFilterPlugin::SetStringOption(const std::string& key, const std::string& value) {
	std::lock_guard<std::mutex> lock(pluginMutex);
	if (setStringFn == NULL)
		return false;
	int result = 0;
	if (!SafePluginSetString(setStringFn, instance, key.c_str(), value.c_str(), &result)) {
		LOG_ERROR("Plugin string option crashed: %s.%s\n", name.c_str(), key.c_str());
		isEnabled = false;
		isValid = false;
		return false;
	}
	return result != 0;
}

void TabletFilterPlugin::Update() {
	std::lock_guard<std::mutex> lock(pluginMutex);
	if (processFn == NULL) {
		position.Set(target);
		return;
	}

	auto now = std::chrono::high_resolution_clock::now();
	double dt = (now - lastTime).count() / 1000000000.0;
	lastTime = now;
	if (firstUpdate || dt <= 0 || dt > 0.1) {
		dt = 0.001;
		firstUpdate = false;
	}

	AetherPluginPoint point = {};
	point.x = target.x;
	point.y = target.y;
	point.z = z;
	point.dt = dt;
	point.isValid = 1;
	point.buttons = buttons;
	point.tipDown = (buttons & 0x01) ? 1 : 0;
	point.pressure = pressure;
	point.hoverDistance = hoverDistance;
	point.tiltX = 0;
	point.tiltY = 0;

	double realX = target.x;
	double realY = target.y;
	double realPressure = pressure;
	BYTE realButtons = buttons;

	if (!SafePluginProcess(processFn, instance, &point)) {
		LOG_ERROR("Plugin process crashed, disabling: %s\n", name.c_str());
		isEnabled = false;
		isValid = false;
		position.Set(target);
		return;
	}

	if (!std::isfinite(point.x) || !std::isfinite(point.y) || fabs(point.x) > 1000000000.0 || fabs(point.y) > 1000000000.0) {
		LOG_ERROR("Plugin returned invalid position, disabling: %s\n", name.c_str());
		isEnabled = false;
		isValid = false;
		position.Set(target);
		return;
	}

	double dx = point.x - realX;
	double dy = point.y - realY;
	double dist = sqrt(dx * dx + dy * dy);
	double maxR = g_pluginSecurity.clampRadiusMm;
	if (maxR < 0.0) maxR = 0.0;
	if (dist > maxR && dist > 0.0) {
		double scale = maxR / dist;
		point.x = realX + dx * scale;
		point.y = realY + dy * scale;
		LOG_WARNING("Plugin output clamped: moved %.2f mm > %.2f mm limit (%s)\n", dist, maxR, name.c_str());
	}

	if (g_pluginSecurity.pressureGate) {
		if (point.pressure < 0.0) point.pressure = 0.0;
		if (point.pressure > realPressure + 1e-9) {
			point.pressure = realPressure;
		}
	}

	if (g_pluginSecurity.buttonGate) {
		BYTE allowed = realButtons;
		if (point.buttons & ~allowed) {
			LOG_WARNING("Plugin tried to synthesize buttons 0x%X (real 0x%X), masked (%s)\n",
				point.buttons, realButtons, name.c_str());
			point.buttons = point.buttons & allowed;
		}
	}

	position.x = point.x;
	position.y = point.y;
	z = point.z;
	pressure = point.pressure;
	hoverDistance = point.hoverDistance;
}
