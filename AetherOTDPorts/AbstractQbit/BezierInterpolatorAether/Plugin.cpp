#include "AetherPluginApi.h"

#include <algorithm>
#include <cmath>
#include <cstring>

struct BezierState {
    double p0x = 0.0, p0y = 0.0;
    double p1x = 0.0, p1y = 0.0;
    double p2x = 0.0, p2y = 0.0;
    double p3x = 0.0, p3y = 0.0;
    bool initialized = false;

    int enabled = 1;
    double strength = 0.45;
    double prediction = 0.35;
    double curve = 0.50;
};

static double Clamp(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void PushSample(BezierState* s, double x, double y) {
    if (!s->initialized) {
        s->p0x = s->p1x = s->p2x = s->p3x = x;
        s->p0y = s->p1y = s->p2y = s->p3y = y;
        s->initialized = true;
        return;
    }
    s->p0x = s->p1x; s->p0y = s->p1y;
    s->p1x = s->p2x; s->p1y = s->p2y;
    s->p2x = s->p3x; s->p2y = s->p3y;
    s->p3x = x;      s->p3y = y;
}

static double Bezier(double p0, double p1, double p2, double p3, double t) {
    double u = 1.0 - t;
    return u * u * u * p0 + 3.0 * u * u * t * p1 + 3.0 * u * t * t * p2 + t * t * t * p3;
}

extern "C" __declspec(dllexport) int __cdecl AetherPluginGetInfo(AetherPluginInfo* info) {
    if (!info) return 0;
    info->apiVersion = AETHER_PLUGIN_API_VERSION;
    info->name = "Bezier Interpolator Aether Port";
    info->description = "Native C++ Aether port inspired by the OpenTabletDriver BezierInterpolator filter.";
    return 1;
}

extern "C" __declspec(dllexport) void* __cdecl AetherPluginCreate() { return new BezierState(); }
extern "C" __declspec(dllexport) void __cdecl AetherPluginDestroy(void* instance) { delete static_cast<BezierState*>(instance); }

extern "C" __declspec(dllexport) void __cdecl AetherPluginReset(void* instance, const AetherPluginPoint* point) {
    BezierState* s = static_cast<BezierState*>(instance);
    if (!s || !point) return;
    s->initialized = false;
    PushSample(s, point->x, point->y);
}

extern "C" __declspec(dllexport) void __cdecl AetherPluginProcess(void* instance, AetherPluginPoint* point) {
    if (!point || !point->isValid) return;
    BezierState fallback;
    BezierState* s = static_cast<BezierState*>(instance ? instance : &fallback);
    if (!s->enabled) return;

    PushSample(s, point->x, point->y);

    double vx = s->p3x - s->p2x;
    double vy = s->p3y - s->p2y;
    double c = Clamp(s->curve, 0.0, 1.0);
    double pred = Clamp(s->prediction, 0.0, 2.0);

    double c1x = s->p1x + (s->p2x - s->p0x) * c;
    double c1y = s->p1y + (s->p2y - s->p0y) * c;
    double c2x = s->p2x + vx * pred * c;
    double c2y = s->p2y + vy * pred * c;

    double t = Clamp(s->strength, 0.01, 1.0);
    point->x = Bezier(s->p1x, c1x, c2x, s->p3x, t);
    point->y = Bezier(s->p1y, c1y, c2y, s->p3y, t);
}

extern "C" __declspec(dllexport) int __cdecl AetherPluginSetDouble(void* instance, const char* key, double value) {
    BezierState* s = static_cast<BezierState*>(instance);
    if (!s || !key) return 0;
    if (std::strcmp(key, "enabled") == 0) { s->enabled = value >= 0.5 ? 1 : 0; return 1; }
    if (std::strcmp(key, "strength") == 0) { s->strength = Clamp(value, 0.01, 1.0); return 1; }
    if (std::strcmp(key, "prediction") == 0) { s->prediction = Clamp(value, 0.0, 2.0); return 1; }
    if (std::strcmp(key, "curve") == 0) { s->curve = Clamp(value, 0.0, 1.0); return 1; }
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
    case 1: info->key = "strength"; info->label = "Strength"; info->type = AETHER_PLUGIN_OPTION_SLIDER; info->minValue = 0.01; info->maxValue = 1.0; info->defaultValue = 0.45; info->format = "%.2f"; return 1;
    case 2: info->key = "prediction"; info->label = "Prediction"; info->type = AETHER_PLUGIN_OPTION_SLIDER; info->minValue = 0.0; info->maxValue = 2.0; info->defaultValue = 0.35; info->format = "%.2f"; return 1;
    case 3: info->key = "curve"; info->label = "Curve"; info->type = AETHER_PLUGIN_OPTION_SLIDER; info->minValue = 0.0; info->maxValue = 1.0; info->defaultValue = 0.50; info->format = "%.2f"; return 1;
    default: return 0;
    }
}
