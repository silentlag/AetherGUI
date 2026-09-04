#include "stdafx.h"
#include "TabletFilterLua.h"
#include "AetherPluginManager.h"

#include <algorithm>
#include <cmath>

#define LOG_MODULE "Lua"
#include "Logger.h"

static void AetherLuaServiceLog(const char* msg) {
	LOG_INFO("%s\n", msg);
}

TabletFilterLua::TabletFilterLua() {
	luaState = NULL;
	stateRef = LUA_NOREF;
}

TabletFilterLua::~TabletFilterLua() {
	UnloadScript();
}

bool TabletFilterLua::LoadScript(const std::wstring& scriptPath) {
	UnloadScript();

	std::string source;
	if (!AetherLuaReadFileBytes(scriptPath, source)) {
		LOG_ERROR("Failed to read Lua plugin: %ls\n", scriptPath.c_str());
		return false;
	}

	g_aetherLuaLog = AetherLuaServiceLog;
	luaState = luaL_newstate();
	if (luaState == NULL) {
		LOG_ERROR("Failed to create Lua state: %ls\n", scriptPath.c_str());
		return false;
	}
	AetherLuaSandbox(luaState);

	std::string error;
	if (!AetherLuaDoBuffer(luaState, source, error)) {
		LOG_ERROR("Lua plugin load error: %ls\n  %s\n", scriptPath.c_str(), error.c_str());
		UnloadScript();
		return false;
	}

	std::string metaName;
	std::string metaDescription;
	std::vector<AetherLuaOption> options;
	AetherLuaReadMetadata(luaState, metaName, metaDescription, options);

	path = scriptPath;
	name = metaName.empty() ? "Lua plugin" : metaName;
	description = metaDescription;

	lua_getglobal(luaState, "aether_create");
	if (lua_isfunction(luaState, -1)) {
		if (lua_pcall(luaState, 0, 1, 0) == LUA_OK) {
			stateRef = luaL_ref(luaState, LUA_REGISTRYINDEX);
		}
		else {
			const char* e = lua_tostring(luaState, -1);
			LOG_ERROR("Lua aether_create crashed: %s (%s)\n", e ? e : "?", name.c_str());
			lua_pop(luaState, 1);
			UnloadScript();
			return false;
		}
	}
	else {
		lua_pop(luaState, 1);
		stateRef = LUA_REFNIL;
	}

	for (size_t i = 0; i < options.size(); i++) {
		ApplyOptionNumber(options[i].key, options[i].defaultValue);
	}

	isEnabled = true;
	isValid = true;
	firstUpdate = true;
	lastTime = std::chrono::high_resolution_clock::now();

	LOG_INFO("Loaded Lua plugin: %s\n", name.c_str());
	return true;
}

void TabletFilterLua::UnloadScript() {
	if (luaState != NULL) {
		if (stateRef != LUA_NOREF && stateRef != LUA_REFNIL)
			luaL_unref(luaState, LUA_REGISTRYINDEX, stateRef);
		lua_close(luaState);
		luaState = NULL;
	}
	stateRef = LUA_NOREF;
	isEnabled = false;
	isValid = false;
}

void TabletFilterLua::SetTarget(Vector2D vector, double h) {
	target.Set(vector);
	z = h;
}

void TabletFilterLua::SetPosition(Vector2D vector, double h) {
	position.Set(vector);
	z = h;
}

void TabletFilterLua::SetReportState(BYTE btns, double press, double hover) {
	buttons = btns;
	pressure = press;
	hoverDistance = hover;
}

bool TabletFilterLua::GetPosition(Vector2D *outputVector) {
	outputVector->Set(position);
	return true;
}

bool TabletFilterLua::ApplyOptionNumber(const std::string& key, double value) {
	if (luaState == NULL)
		return false;

	lua_getglobal(luaState, "aether_set");
	if (!lua_isfunction(luaState, -1)) {
		lua_pop(luaState, 1);
		return false;
	}
	if (stateRef != LUA_NOREF && stateRef != LUA_REFNIL)
		lua_rawgeti(luaState, LUA_REGISTRYINDEX, stateRef);
	else
		lua_pushnil(luaState);
	lua_pushstring(luaState, key.c_str());
	lua_pushnumber(luaState, value);
	if (lua_pcall(luaState, 3, 0, 0) != LUA_OK) {
		const char* e = lua_tostring(luaState, -1);
		LOG_ERROR("Lua aether_set crashed: %s.%s (%s)\n", name.c_str(), key.c_str(), e ? e : "?");
		lua_pop(luaState, 1);
		isEnabled = false;
		isValid = false;
		return false;
	}
	return true;
}

bool TabletFilterLua::SetDoubleOption(const std::string& key, double value) {
	std::lock_guard<std::mutex> lock(pluginMutex);
	return ApplyOptionNumber(key, value);
}

bool TabletFilterLua::SetStringOption(const std::string& key, const std::string& value) {
	(void)key;
	(void)value;
	return false;
}

void TabletFilterLua::Reset(Vector2D pos) {
	std::lock_guard<std::mutex> lock(pluginMutex);
	position.Set(pos);
	target.Set(pos);
	firstUpdate = true;
	lastTime = std::chrono::high_resolution_clock::now();

	if (luaState == NULL)
		return;

	lua_getglobal(luaState, "aether_reset");
	if (!lua_isfunction(luaState, -1)) {
		lua_pop(luaState, 1);
		return;
	}
	if (stateRef != LUA_NOREF && stateRef != LUA_REFNIL)
		lua_rawgeti(luaState, LUA_REGISTRYINDEX, stateRef);
	else
		lua_pushnil(luaState);
	AetherLuaPushPoint(luaState, pos.x, pos.y, z, 0, 1, buttons, (buttons & 0x01) ? 1 : 0, pressure, hoverDistance, 0, 0);
	if (lua_pcall(luaState, 2, 0, 0) != LUA_OK) {
		const char* e = lua_tostring(luaState, -1);
		LOG_ERROR("Lua aether_reset crashed: %s (%s)\n", name.c_str(), e ? e : "?");
		lua_pop(luaState, 1);
		isEnabled = false;
		isValid = false;
	}
}

void TabletFilterLua::Update() {
	std::lock_guard<std::mutex> lock(pluginMutex);
	if (luaState == NULL) {
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

	// remember what the pen actually reported, the script only gets to smooth it, not fake it
	double realX = target.x;
	double realY = target.y;
	double realPressure = pressure;
	BYTE realButtons = buttons;

	lua_getglobal(luaState, "aether_process");
	if (!lua_isfunction(luaState, -1)) {
		lua_pop(luaState, 1);
		position.Set(target);
		return;
	}
	if (stateRef != LUA_NOREF && stateRef != LUA_REFNIL)
		lua_rawgeti(luaState, LUA_REGISTRYINDEX, stateRef);
	else
		lua_pushnil(luaState);
	AetherLuaPushPoint(luaState, realX, realY, z, dt, 1, buttons, (buttons & 0x01) ? 1 : 0, pressure, hoverDistance, 0, 0);

	if (lua_pcall(luaState, 2, 1, 0) != LUA_OK) {
		const char* e = lua_tostring(luaState, -1);
		LOG_ERROR("Lua aether_process crashed, disabling: %s (%s)\n", name.c_str(), e ? e : "?");
		lua_pop(luaState, 1);
		isEnabled = false;
		isValid = false;
		position.Set(target);
		return;
	}

	double outX = realX;
	double outY = realY;
	double outPressure = realPressure;
	BYTE outButtons = realButtons;
	double outHover = hoverDistance;

	if (lua_istable(luaState, -1)) {
		outX = AetherLuaPointNumber(luaState, "x", realX);
		outY = AetherLuaPointNumber(luaState, "y", realY);
		outPressure = AetherLuaPointNumber(luaState, "pressure", realPressure);
		outHover = AetherLuaPointNumber(luaState, "hoverDistance", hoverDistance);
		lua_getfield(luaState, -1, "buttons");
		if (lua_type(luaState, -1) == LUA_TNUMBER)
			outButtons = (BYTE)(int)lua_tonumber(luaState, -1);
		lua_pop(luaState, 1);
	}
	lua_pop(luaState, 1);

	if (!std::isfinite(outX) || !std::isfinite(outY) || fabs(outX) > 1000000000.0 || fabs(outY) > 1000000000.0) {
		LOG_ERROR("Lua plugin returned invalid position, disabling: %s\n", name.c_str());
		isEnabled = false;
		isValid = false;
		position.Set(target);
		return;
	}

	double dx = outX - realX;
	double dy = outY - realY;
	double dist = sqrt(dx * dx + dy * dy);
	double maxR = g_pluginSecurity.clampRadiusMm;
	if (maxR < 0.0) maxR = 0.0;
	if (dist > maxR && dist > 0.0) {
		double scale = maxR / dist;
		outX = realX + dx * scale;
		outY = realY + dy * scale;
		LOG_WARNING("Lua plugin output clamped: moved %.2f mm > %.2f mm limit (%s)\n", dist, maxR, name.c_str());
	}

	if (g_pluginSecurity.pressureGate) {
		if (outPressure < 0.0) outPressure = 0.0;
		if (outPressure > realPressure + 1e-9) {
			outPressure = realPressure;
		}
	}

	if (g_pluginSecurity.buttonGate) {
		BYTE allowed = realButtons;
		if (outButtons & ~allowed) {
			LOG_WARNING("Lua plugin tried to synthesize buttons 0x%X (real 0x%X), masked (%s)\n",
				outButtons, realButtons, name.c_str());
			outButtons = outButtons & allowed;
		}
	}

	position.x = outX;
	position.y = outY;
	pressure = outPressure;
	hoverDistance = outHover;
}
