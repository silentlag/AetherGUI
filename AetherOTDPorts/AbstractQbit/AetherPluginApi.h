#pragma once

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

#ifdef __cplusplus
}
#endif
