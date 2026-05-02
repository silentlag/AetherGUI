# Aether Plugin Porting Notes

OpenTabletDriver plugins are .NET assemblies built against `OpenTabletDriver.Plugin`.
Aether is a native C++ driver, so OTD DLLs cannot be loaded directly by the current
native plugin loader. Port the filter algorithm into a native Aether DLL instead.

## OTD to Aether mapping

- OTD `IPositionedPipelineElement<IDeviceReport>.Consume(report)` maps to
  `AetherPluginProcess(void* instance, AetherPluginPoint* point)`.
- OTD `report.Position.X/Y` maps to `point->x` and `point->y`.
- OTD per-filter state maps to the pointer returned by `AetherPluginCreate`.
- OTD settings/properties map to optional exports:
  `AetherPluginSetDouble` and `AetherPluginSetString`.
- Aether loads native DLL filters from `bin/Release/plugins/<plugin-name>/`.

## Minimal native filter

```cpp
#include "AetherPluginApi.h"

struct FilterState {
    double strength = 1.0;
};

extern "C" __declspec(dllexport)
int __cdecl AetherPluginGetInfo(AetherPluginInfo* info) {
    info->apiVersion = AETHER_PLUGIN_API_VERSION;
    info->name = "Example Native Filter";
    info->description = "Minimal Aether packet filter";
    return 1;
}

extern "C" __declspec(dllexport)
void* __cdecl AetherPluginCreate() {
    return new FilterState();
}

extern "C" __declspec(dllexport)
void __cdecl AetherPluginDestroy(void* instance) {
    delete static_cast<FilterState*>(instance);
}

extern "C" __declspec(dllexport)
int __cdecl AetherPluginSetDouble(void* instance, const char* key, double value) {
    FilterState* state = static_cast<FilterState*>(instance);
    if (strcmp(key, "strength") == 0) {
        state->strength = value;
        return 1;
    }
    return 0;
}

extern "C" __declspec(dllexport)
void __cdecl AetherPluginProcess(void* instance, AetherPluginPoint* point) {
    FilterState* state = static_cast<FilterState*>(instance);
    point->x *= state->strength;
    point->y *= state->strength;
}
```

## Runtime commands

- `PluginInstall <dll-path>` installs and reloads a native plugin DLL.
- `PluginReload` reloads the plugin folder.
- `PluginList` prints loaded plugins to the console.
- `PluginEnable <index> on|off` toggles a loaded plugin.
- `PluginSet <index> <key> <value>` sends a numeric or string setting to a plugin.
