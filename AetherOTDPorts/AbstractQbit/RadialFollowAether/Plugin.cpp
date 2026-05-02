#include "AetherPluginApi.h"

#include <algorithm>
#include <cmath>
#include <cstring>

struct RadialState {
    double outX = 0.0;
    double outY = 0.0;
    bool initialized = false;

    int enabled = 1;
    double radius = 0.45;
    double softness = 0.65;
    double snap = 0.0;
};

static double Clamp(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

extern "C" __declspec(dllexport) int __cdecl AetherPluginGetInfo(AetherPluginInfo* info) {
    if (!info) return 0;
    info->apiVersion = AETHER_PLUGIN_API_VERSION;
    info->name = "Radial Follow Aether Port";
    info->description = "Native C++ Aether port inspired by the OpenTabletDriver RadialFollow filter.";
    return 1;
}

extern "C" __declspec(dllexport) void* __cdecl AetherPluginCreate() { return new RadialState(); }
extern "C" __declspec(dllexport) void __cdecl AetherPluginDestroy(void* instance) { delete static_cast<RadialState*>(instance); }

extern "C" __declspec(dllexport) void __cdecl AetherPluginReset(void* instance, const AetherPluginPoint* point) {
    RadialState* s = static_cast<RadialState*>(instance);
    if (!s || !point) return;
    s->outX = point->x;
    s->outY = point->y;
    s->initialized = true;
}

extern "C" __declspec(dllexport) void __cdecl AetherPluginProcess(void* instance, AetherPluginPoint* point) {
    if (!point || !point->isValid) return;
    RadialState fallback;
    RadialState* s = static_cast<RadialState*>(instance ? instance : &fallback);
    if (!s->enabled) return;

    if (!s->initialized) {
        s->outX = point->x;
        s->outY = point->y;
        s->initialized = true;
        return;
    }

    double dx = point->x - s->outX;
    double dy = point->y - s->outY;
    double dist = std::sqrt(dx * dx + dy * dy);
    double radius = Clamp(s->radius, 0.0, 20.0);

    if (dist <= s->snap) {
        point->x = s->outX;
        point->y = s->outY;
        return;
    }

    if (radius <= 0.0001 || dist > radius) {
        double excess = dist - radius;
        if (excess < 0.0) excess = 0.0;
        double follow = radius <= 0.0001 ? 1.0 : Clamp(excess / dist, 0.0, 1.0);
        follow = Clamp(follow + (1.0 - follow) * Clamp(s->softness, 0.0, 1.0), 0.0, 1.0);
        s->outX += dx * follow;
        s->outY += dy * follow;
    }

    point->x = s->outX;
    point->y = s->outY;
}

extern "C" __declspec(dllexport) int __cdecl AetherPluginSetDouble(void* instance, const char* key, double value) {
    RadialState* s = static_cast<RadialState*>(instance);
    if (!s || !key) return 0;
    if (std::strcmp(key, "enabled") == 0) { s->enabled = value >= 0.5 ? 1 : 0; return 1; }
    if (std::strcmp(key, "radius") == 0) { s->radius = Clamp(value, 0.0, 20.0); return 1; }
    if (std::strcmp(key, "softness") == 0) { s->softness = Clamp(value, 0.0, 1.0); return 1; }
    if (std::strcmp(key, "snap") == 0) { s->snap = Clamp(value, 0.0, 10.0); return 1; }
    return 0;
}

extern "C" __declspec(dllexport) int __cdecl AetherPluginSetString(void* instance, const char* key, const char* value) {
    if (!instance || !key || !value) return 0;
    if (std::strcmp(key, "enabled") == 0) {
        return AetherPluginSetDouble(instance, key, (_stricmp(value, "true") == 0 || _stricmp(value, "on") == 0 || std::strcmp(value, "1") == 0) ? 1.0 : 0.0);
    }
    return 0;
}

extern "C" __declspec(dllexport) int __cdecl AetherPluginGetOptionCount() { return 4; }

extern "C" __declspec(dllexport) int __cdecl AetherPluginGetOptionInfo(int index, AetherPluginOptionInfo* info) {
    if (!info) return 0;
    info->apiVersion = AETHER_PLUGIN_API_VERSION;
    info->description = "";
    switch (index) {
    case 0: info->key = "enabled"; info->label = "Enabled"; info->type = AETHER_PLUGIN_OPTION_TOGGLE; info->minValue = 0; info->maxValue = 1; info->defaultValue = 1; info->format = "%.0f"; return 1;
    case 1: info->key = "radius"; info->label = "Radius"; info->type = AETHER_PLUGIN_OPTION_SLIDER; info->minValue = 0.0; info->maxValue = 20.0; info->defaultValue = 0.45; info->format = "%.2f"; return 1;
    case 2: info->key = "softness"; info->label = "Softness"; info->type = AETHER_PLUGIN_OPTION_SLIDER; info->minValue = 0.0; info->maxValue = 1.0; info->defaultValue = 0.65; info->format = "%.2f"; return 1;
    case 3: info->key = "snap"; info->label = "Snap"; info->type = AETHER_PLUGIN_OPTION_SLIDER; info->minValue = 0.0; info->maxValue = 10.0; info->defaultValue = 0.0; info->format = "%.2f"; return 1;
    default: return 0;
    }
}
