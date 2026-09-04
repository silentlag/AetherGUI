# Aether Native Plugin API

AetherGUI lets you write your own pen filters as native DLLs or Lua scripts. A plugin plugs into the driver's timed filter chain (the same stage as Smoothing / Reconstructor / Temporal Resampler) and can modify the position, pressure, and other fields of each pen point.

- API version: `1`
- Languages: C or C++ (x64) for DLLs, Lua 5.4 for script filters
- Install folder: `<AetherGUI folder>\plugins\`
- Working examples: `plugins-example/` in the repository root
- Full reference: this file

---

## Quick start (C / C++)

1. Copy the `plugins-example` folder anywhere you like.
2. Open AetherGUI -> Filters -> **Build Source** and select that folder.
   The GUI finds `build_aether_plugin.ps1`, compiles the DLL through Visual Studio, and installs it for you.
3. Press **Reload** in the plugins section.

Requirements: Visual Studio 2019/2022 with the "Desktop development with C++" workload (`cl.exe`).

Manual build without the GUI:

```bat
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /O2 /LD /EHsc moving_average.cpp /Fe:moving_average.dll
```

Alternatively, install a prebuilt DLL with the **Install DLL** button - it lands in `plugins\<name>\`.

## Quick start (Lua)

No compiler needed. Drop a `.lua` file into `plugins\<name>\<name>.lua` (or use the **Install Lua** button in AetherGUI), press **Reload**, and the filter appears with its own sliders and toggles. Edit the text file, reload, done.

---

## DLL contract

The DLL must export (cdecl, x64):

### Required

```c
int  AetherPluginGetInfo(AetherPluginInfo* info);
void AetherPluginProcess(void* instance, AetherPluginPoint* point);
```

### Optional

```c
void* AetherPluginCreate();
void  AetherPluginDestroy(void* instance);
void  AetherPluginReset(void* instance, const AetherPluginPoint* point);
int   AetherPluginSetDouble(void* instance, const char* key, double value);
int   AetherPluginSetString(void* instance, const char* key, const char* value);
int   AetherPluginGetOptionCount();
int   AetherPluginGetOptionInfo(int index, AetherPluginOptionInfo* info);
```

`AetherPluginCreate` is called once on load, `Destroy` on unload. If `Create` is not exported, every call receives `instance == NULL`.

---

## Structures

```c
typedef struct AetherPluginInfo {
	int apiVersion;      // AETHER_PLUGIN_API_VERSION (1)
	const char* name;    // shown in the GUI
	const char* description;
} AetherPluginInfo;

typedef struct AetherPluginPoint {
	double x, y;         // pen position in mm, tablet coordinates
	double z;            // height / raw sensor data when available
	double dt;           // seconds since the previous call
	int    isValid;      // always 1 in Process
	int    buttons;      // bitmask: 0x01 tip, 0x02 lower, 0x04 upper
	int    tipDown;      // 1 while the tip is pressed
	double pressure;     // 0.0 .. 1.0 (normalized)
	double hoverDistance;// hover height when reported
	double tiltX, tiltY; // tilt (always 0 for now)
} AetherPluginPoint;

typedef struct AetherPluginOptionInfo {
	int apiVersion;
	const char* key;         // option id for AetherPluginSetDouble
	const char* label;       // GUI label
	int type;                // AETHER_PLUGIN_OPTION_SLIDER or _TOGGLE
	double minValue, maxValue;
	double defaultValue;
	const char* format;      // printf format, e.g. "%.2f"
	const char* description; // tooltip
} AetherPluginOptionInfo;
```

---

## Life cycle

```
GetInfo -> Create -> (SetDouble xN for each option) -> Reset* -> Process xN -> Destroy
```

- `Process` runs on every filter timer tick (up to 2000 Hz with interpolation enabled). Keep it fast: no allocations, no locks, no I/O.
- `Reset` is called when the pen leaves range, the filter is disabled, or lag accumulates - clear your internal buffers.
- `dt` is 0.001 on the first call and after a pause.

## What you may change in Process

Mutate the fields of `point` in place - the result continues down the chain:

- `x`, `y` - position (mm). Output is clamped to `clampRadiusMm` (default 50 mm) around the real point.
- `pressure` - with `pressureGate` on (default) the output can never exceed the real pressure.
- `buttons` - with `buttonGate` on (default) a plugin cannot add buttons that were not physically pressed; it can only release them.

## Safety and stability

- Every plugin call is wrapped in SEH: a crash inside the DLL cannot take down the driver - the plugin is disabled and logged.
- Non-numeric (NaN/Inf) or huge output coordinates disable the plugin.
- An `apiVersion` mismatch prevents the DLL from loading at all.
- Allowlist: see the `PluginSecurity` / `PluginHash` commands below.

---

## Driver commands (GUI console)

```
PluginInstall "<path to DLL or folder>"
PluginReload
PluginList
PluginEnable <name|index> <on|off>
PluginSet <name|index> <key> <value>
PluginDir
PluginSecurity <clampRadiusMm> <pressureGate 0|1> <buttonGate 0|1> <allowlist 0|1>
PluginHash <name|index>
PluginAllowlistReload
```

---

## Full C example

See `plugins-example/moving_average.cpp` - a working moving-average filter (~150 lines) with one slider option and one-click building through Build Source.

To write your own: copy the example, rename it, and edit `AetherPluginProcess` - that function is your filter.

---

# Lua plugins (no compiler)

AetherGUI also supports filters written in Lua 5.4. The script is placed at `plugins\<name>\<name>.lua` and is picked up automatically on Reload / restart. No Visual Studio involved - edit a text file and reload the plugins.

The Lua contract mirrors the C API:

```lua
function aether_info()
    return { name = "My Filter", description = "..." }
end

function aether_options()   -- optional
    return {
        { key = "strength", label = "Strength", type = "slider",
          min = 0, max = 1, default = 0.5,
          format = "%.2f", description = "..." },
        { key = "invert", label = "Invert", type = "toggle",
          default = false, description = "..." },
    }
end

function aether_create()    -- optional; return any state you like
    return { x = 0, y = 0 }
end

function aether_set(state, key, value)  -- called when options change
    state[key] = value
    return true
end

function aether_reset(state, point)    -- optional; reset state
end

function aether_process(state, point) -- REQUIRED; mutates point
    point.x = (point.x + state.x) * 0.5
    state.x = point.x
    return point   -- must return point (or nil for no change)
end
```

`point` fields: `x`, `y`, `z`, `dt`, `isValid`, `buttons`, `tipDown`, `pressure`, `hoverDistance`, `tiltX`, `tiltY`. Same names as the C API.

## Sandbox

- Available: `math`, `string`, `table`, `coroutine`, `utf8`, `os.clock`, `os.time`.
- **Removed**: `io`, `package`, `debug`, `loadfile`, `dofile`, `require`, `load`.
- `print(...)` goes to the driver log (`[Lua] ...`) and does not pollute the GUI protocol.
- `aether.log(...)` does the same.

## Errors and safety

- An error or crash in any function disables the plugin and writes a log line (`[Lua] aether_process crashed: ...`).
- Non-numeric or huge output coordinates disable the plugin.
- The same gates as DLLs apply: 50 mm clamp radius (configurable via `PluginSecurity`), pressure never above the real value, buttons only from the physically pressed set.
- Locking: `aether_process` / `aether_set` / `aether_reset` share one plugin mutex - avoid heavy computation (they can be called up to 2000 times per second with interpolation).

## Example

`plugins-example/moving_average.lua` - the same moving average as the C version, in Lua (~60 lines). Install:

1. Copy the `plugins-example` folder to `<AetherGUI folder>\plugins\moving_average\` (any folder name works).
2. Put `moving_average.lua` inside.
3. Filters -> **Reload**. The plugin appears in the list with its Window option.

Or press **Install Lua** in the GUI and pick the `.lua` file - AetherGUI copies it into `plugins\<name>\<name>.lua` and reloads for you.

C and Lua plugins can coexist in `plugins\` and run in one load order (DLLs first, then Lua per folder).
