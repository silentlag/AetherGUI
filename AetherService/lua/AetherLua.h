#pragma once

#include <string>
#include <vector>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

struct AetherLuaOption {
	std::string key;
	std::string label;
	std::string format;
	std::string description;
	int type = 0;
	double minValue = 0.0;
	double maxValue = 1.0;
	double defaultValue = 0.0;
};

typedef void (*AetherLuaLogFn)(const char* msg);

static AetherLuaLogFn g_aetherLuaLog = nullptr;

inline int aether_lua_log_impl(lua_State* L) {
	int n = lua_gettop(L);
	if (g_aetherLuaLog != nullptr && n >= 1) {
		const char* s = luaL_tolstring(L, 1, nullptr);
		g_aetherLuaLog(s ? s : "");
		lua_pop(L, 1);
	}
	return 0;
}

inline void AetherLuaSandbox(lua_State* L) {
	luaL_openlibs(L);

	lua_pushnil(L); lua_setglobal(L, "io");
	lua_pushnil(L); lua_setglobal(L, "package");
	lua_pushnil(L); lua_setglobal(L, "debug");
	lua_pushnil(L); lua_setglobal(L, "loadfile");
	lua_pushnil(L); lua_setglobal(L, "dofile");
	lua_pushnil(L); lua_setglobal(L, "require");
	lua_pushnil(L); lua_setglobal(L, "load");

	lua_getglobal(L, "os");
	if (lua_istable(L, -1)) {
		lua_createtable(L, 0, 2);
		lua_getfield(L, -2, "clock"); lua_setfield(L, -2, "clock");
		lua_getfield(L, -2, "time");  lua_setfield(L, -2, "time");
		lua_setglobal(L, "os");
	}
	lua_pop(L, 1);

	lua_pushcfunction(L, aether_lua_log_impl);
	lua_setglobal(L, "print");

	lua_newtable(L);
	lua_pushcfunction(L, aether_lua_log_impl);
	lua_setfield(L, -2, "log");
	lua_setglobal(L, "aether");
}

#if defined(_WIN32)
inline bool AetherLuaReadFileBytes(const std::wstring& path, std::string& out) {
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD size = GetFileSize(h, NULL);
	if (size == INVALID_FILE_SIZE || size > 16 * 1024 * 1024) { CloseHandle(h); return false; }
	out.resize(size);
	DWORD read = 0;
	BOOL ok = size > 0 ? ReadFile(h, &out[0], size, &read, NULL) : TRUE;
	CloseHandle(h);
	return ok && read == size;
}
#else
inline bool AetherLuaReadFileBytes(const std::wstring& path, std::string& out);
#endif

inline bool AetherLuaDoBuffer(lua_State* L, const std::string& source, std::string& error) {
	if (luaL_loadbuffer(L, source.data(), source.size(), "aether_plugin") != LUA_OK) {
		const char* e = lua_tostring(L, -1);
		error = e ? e : "load error";
		lua_pop(L, 1);
		return false;
	}
	if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
		const char* e = lua_tostring(L, -1);
		error = e ? e : "run error";
		lua_pop(L, 1);
		return false;
	}
	return true;
}

inline bool AetherLuaGetStringField(lua_State* L, int idx, const char* key, std::string& out) {
	lua_getfield(L, idx, key);
	if (lua_type(L, -1) == LUA_TSTRING) {
		size_t len = 0;
		const char* s = lua_tolstring(L, -1, &len);
		out.assign(s ? s : "", len);
		lua_pop(L, 1);
		return true;
	}
	lua_pop(L, 1);
	return false;
}

inline double AetherLuaGetNumberField(lua_State* L, int idx, const char* key, double fallback) {
	lua_getfield(L, idx, key);
	double v = (lua_type(L, -1) == LUA_TNUMBER) ? lua_tonumber(L, -1) : fallback;
	lua_pop(L, 1);
	return v;
}

inline bool AetherLuaReadMetadata(lua_State* L, std::string& name, std::string& description, std::vector<AetherLuaOption>& options) {
	name.clear();
	description.clear();
	options.clear();

	lua_getglobal(L, "aether_info");
	if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return false; }
	if (lua_pcall(L, 0, 1, 0) != LUA_OK) { lua_pop(L, 1); return false; }
	if (lua_istable(L, -1)) {
		AetherLuaGetStringField(L, -1, "name", name);
		AetherLuaGetStringField(L, -1, "description", description);
	}
	lua_pop(L, 1);

	lua_getglobal(L, "aether_options");
	if (lua_isfunction(L, -1) && lua_pcall(L, 0, 1, 0) == LUA_OK && lua_istable(L, -1)) {
		int len = (int)lua_rawlen(L, -1);
		for (int i = 1; i <= len && i <= 64; i++) {
			lua_rawgeti(L, -1, i);
			if (lua_istable(L, -1)) {
				AetherLuaOption option;
				AetherLuaGetStringField(L, -1, "key", option.key);
				AetherLuaGetStringField(L, -1, "label", option.label);
				AetherLuaGetStringField(L, -1, "format", option.format);
				AetherLuaGetStringField(L, -1, "description", option.description);
				option.type = (AetherLuaGetNumberField(L, -1, "type", 0.0) >= 1.0) ? 1 : 0;
				option.minValue = AetherLuaGetNumberField(L, -1, "min", 0.0);
				option.maxValue = AetherLuaGetNumberField(L, -1, "max", 1.0);
				option.defaultValue = AetherLuaGetNumberField(L, -1, "default", option.minValue);
				if (option.maxValue < option.minValue) {
					double t = option.minValue; option.minValue = option.maxValue; option.maxValue = t;
				}
				if (!option.key.empty())
					options.push_back(option);
			}
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
	return !name.empty() || !options.empty();
}

inline void AetherLuaPushPoint(lua_State* L, double x, double y, double z, double dt,
	int isValid, int buttons, int tipDown, double pressure, double hoverDistance, double tiltX, double tiltY) {
	lua_createtable(L, 0, 11);
	auto num = [&](const char* k, double v) { lua_pushnumber(L, v); lua_setfield(L, -2, k); };
	auto integer = [&](const char* k, int v) { lua_pushinteger(L, v); lua_setfield(L, -2, k); };
	num("x", x); num("y", y); num("z", z); num("dt", dt);
	integer("isValid", isValid); integer("buttons", buttons); integer("tipDown", tipDown);
	num("pressure", pressure); num("hoverDistance", hoverDistance);
	num("tiltX", tiltX); num("tiltY", tiltY);
}

inline double AetherLuaPointNumber(lua_State* L, const char* key, double fallback) {
	lua_getfield(L, -1, key);
	double v = (lua_type(L, -1) == LUA_TNUMBER) ? lua_tonumber(L, -1) : fallback;
	lua_pop(L, 1);
	return v;
}
