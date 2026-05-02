#include "stdafx.h"
#include "TabletFilterPlugin.h"

#define LOG_MODULE "Plugin"
#include "Logger.h"

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
		LOG_ERROR("Failed to load plugin DLL: %ls\n", dllPath.c_str());
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

	if (createFn != NULL)
		instance = createFn();

	isEnabled = true;
	isValid = true;
	firstUpdate = true;
	lastTime = std::chrono::high_resolution_clock::now();

	LOG_INFO("Loaded plugin: %s\n", name.c_str());
	return true;
}

void TabletFilterPlugin::Unload() {
	if (module != NULL) {
		if (destroyFn != NULL && instance != NULL)
			destroyFn(instance);

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
		resetFn(instance, &point);
	}
}

bool TabletFilterPlugin::SetDoubleOption(const std::string& key, double value) {
	if (setDoubleFn == NULL)
		return false;
	return setDoubleFn(instance, key.c_str(), value) != 0;
}

bool TabletFilterPlugin::SetStringOption(const std::string& key, const std::string& value) {
	if (setStringFn == NULL)
		return false;
	return setStringFn(instance, key.c_str(), value.c_str()) != 0;
}

void TabletFilterPlugin::Update() {
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

	processFn(instance, &point);

	position.x = point.x;
	position.y = point.y;
	z = point.z;
	pressure = point.pressure;
	hoverDistance = point.hoverDistance;
}
