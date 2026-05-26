#pragma once

#if defined(_WIN32)
	#define AETHER_PLUGIN_CALL __cdecl
#else
	#define AETHER_PLUGIN_CALL
#endif

#define AETHER_PLUGIN_API_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AetherPluginInfo {
	int apiVersion;
	const char* name;
	const char* description;
} AetherPluginInfo;

enum AetherPluginOptionType {
	AETHER_PLUGIN_OPTION_SLIDER = 0,
	AETHER_PLUGIN_OPTION_TOGGLE = 1
};

typedef struct AetherPluginOptionInfo {
	int apiVersion;
	const char* key;
	const char* label;
	int type;
	double minValue;
	double maxValue;
	double defaultValue;
	const char* format;
	const char* description;
} AetherPluginOptionInfo;

typedef struct AetherPluginPoint {
	double x;
	double y;
	double z;
	double dt;
	int isValid;
	int buttons;
	int tipDown;
	double pressure;
	double hoverDistance;
	double tiltX;
	double tiltY;
} AetherPluginPoint;

typedef int  (AETHER_PLUGIN_CALL *AetherPluginGetInfoFn)(AetherPluginInfo* info);
typedef void*(AETHER_PLUGIN_CALL *AetherPluginCreateFn)();
typedef void (AETHER_PLUGIN_CALL *AetherPluginDestroyFn)(void* instance);
typedef void (AETHER_PLUGIN_CALL *AetherPluginResetFn)(void* instance, const AetherPluginPoint* point);
typedef void (AETHER_PLUGIN_CALL *AetherPluginProcessFn)(void* instance, AetherPluginPoint* point);
typedef int  (AETHER_PLUGIN_CALL *AetherPluginSetDoubleFn)(void* instance, const char* key, double value);
typedef int  (AETHER_PLUGIN_CALL *AetherPluginSetStringFn)(void* instance, const char* key, const char* value);
typedef int  (AETHER_PLUGIN_CALL *AetherPluginGetOptionCountFn)();
typedef int  (AETHER_PLUGIN_CALL *AetherPluginGetOptionInfoFn)(int index, AetherPluginOptionInfo* info);

#ifdef __cplusplus
}
#endif
