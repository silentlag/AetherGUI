#include "stdafx.h"
#include "TabletFilterPlugin.h"

#define LOG_MODULE "Plugin"
#include "Logger.h"

static void* SafePluginCreate(AetherPluginCreateFn fn) {
	if (fn == NULL)
		return NULL;
	__try {
		return fn();
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return NULL;
	}
}

static void SafePluginDestroy(AetherPluginDestroyFn fn, void* instance, bool* crashed) {
	if (crashed) *crashed = false;
	if (fn == NULL || instance == NULL)
		return;
	__try {
		fn(instance);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		if (crashed) *crashed = true;
	}
}

static bool SafePluginReset(AetherPluginResetFn fn, void* instance, const AetherPluginPoint* point) {
	if (fn == NULL)
		return true;
	__try {
		fn(instance, point);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

static bool SafePluginSetDouble(AetherPluginSetDoubleFn fn, void* instance, const char* key, double value, int* result) {
	if (result) *result = 0;
	if (fn == NULL)
		return true;
	__try {
		if (result) *result = fn(instance, key, value);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

static bool SafePluginSetString(AetherPluginSetStringFn fn, void* instance, const char* key, const char* value, int* result) {
	if (result) *result = 0;
	if (fn == NULL)
		return true;
	__try {
		if (result) *result = fn(instance, key, value);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

static bool SafePluginProcess(AetherPluginProcessFn fn, void* instance, AetherPluginPoint* point) {
	if (fn == NULL)
		return true;
	__try {
		fn(instance, point);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

TabletFilterPlugin::TabletFilterPlugin() {
	module = NULL;
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

	module = LoadLibraryW(dllPath.c_str());
	if (module == NULL) {
		LOG_ERROR("Failed to load plugin DLL: %ls (error %lu)\n", dllPath.c_str(), GetLastError());
		return false;
	}

	AetherPluginGetInfoFn getInfoFn = (AetherPluginGetInfoFn)GetProcAddress(module, "AetherPluginGetInfo");
	AetherPluginCreateFn createFn = (AetherPluginCreateFn)GetProcAddress(module, "AetherPluginCreate");
	destroyFn = (AetherPluginDestroyFn)GetProcAddress(module, "AetherPluginDestroy");
	resetFn = (AetherPluginResetFn)GetProcAddress(module, "AetherPluginReset");
	processFn = (AetherPluginProcessFn)GetProcAddress(module, "AetherPluginProcess");
	setDoubleFn = (AetherPluginSetDoubleFn)GetProcAddress(module, "AetherPluginSetDouble");
	setStringFn = (AetherPluginSetStringFn)GetProcAddress(module, "AetherPluginSetString");

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
	if (module != NULL) {
		bool destroyCrashed = false;
		SafePluginDestroy(destroyFn, instance, &destroyCrashed);
		if (destroyCrashed)
			LOG_ERROR("Plugin destroy crashed: %ls\n", path.c_str());

		FreeLibrary(module);
	}

	module = NULL;
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

	position.x = point.x;
	position.y = point.y;
	z = point.z;
	pressure = point.pressure;
	hoverDistance = point.hoverDistance;
}
