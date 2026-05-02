# AbstractQbit Aether OTD Native Ports

Native C++ Aether-compatible ports of AbstractQbit OpenTabletDriver-style filters.

Included ports:

- `BezierInterpolatorAether`
- `RadialFollowAether`

These plugins export the Aether native plugin API and can be installed through AetherGUI as DLL filters.

## Recommended: ship prebuilt DLLs

AetherGUI checks for a valid Aether DLL in the selected/downloaded port folder before trying to build source.
If a DLL is present and exports the required Aether API, it will be installed directly without running `build.ps1`.

Recommended GitHub layout:

```text
AetherOTDPorts/
  AbstractQbit/
    BezierInterpolatorAether/
      BezierInterpolatorAether.dll
      Plugin.cpp
      BezierInterpolatorAether.vcxproj
    RadialFollowAether/
      RadialFollowAether.dll
      Plugin.cpp
      RadialFollowAether.vcxproj
    AetherPluginApi.h
    AbstractOTDPlugins.sln
    build.ps1
```

DLLs can also be left in the Visual Studio output folders:

```text
BezierInterpolatorAether/bin/Release/BezierInterpolatorAether.dll
RadialFollowAether/bin/Release/RadialFollowAether.dll
```

## Build with Visual Studio

1. Open:

   ```text
   AbstractOTDPlugins.sln
   ```

2. Select:

   ```text
   Release | x64
   ```

3. Run:

   ```text
   Build Solution
   ```

Expected output:

```text
BezierInterpolatorAether/bin/Release/BezierInterpolatorAether.dll
RadialFollowAether/bin/Release/RadialFollowAether.dll
```

## Build with PowerShell

From this folder:

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

Requirements:

- Visual Studio or Visual Studio Build Tools
- Desktop development with C++ workload
- MSBuild
- MSVC C++ toolset
- Windows 10/11 SDK

The script attempts to find MSBuild automatically and selects a matching platform toolset:

- Visual Studio 18 -> `v145`
- Visual Studio 2022 -> `v143`
- Visual Studio 2019 -> `v142`

If MSBuild is not found, run the script from **Developer PowerShell for Visual Studio** or install Visual Studio Build Tools.

## Install through AetherGUI

### From local source

1. Open `AetherGUI.exe`.
2. Go to `Filters` -> `Plugins`.
3. Press `Build Source`.
4. Select either:
   - this `AbstractQbit` folder to build both ports, or
   - `BezierInterpolatorAether/`, or
   - `RadialFollowAether/`.
5. AetherGUI finds a DLL first. If no DLL is available, it tries to build the source.
6. The produced/found DLL is installed into `plugins/` and filters are reloaded.

### From GitHub OTD Ports catalog

1. Put this folder under:

   ```text
   AetherOTDPorts/AbstractQbit/
   ```

2. Open AetherGUI.
3. Open `Plugin Manager`.
4. Go to `OTD Ports`.
5. Press `Refresh`.
6. Select `BezierInterpolatorAether` or `RadialFollowAether`.
7. Press `Install`.

If a prebuilt DLL is present in the port folder, AetherGUI installs it directly. Otherwise, it downloads the source and attempts to build it locally.

## Aether plugin API requirements

A valid DLL must export at least:

```cpp
AetherPluginGetInfo
AetherPluginProcess
```

Optional exports used for settings/options:

```cpp
AetherPluginCreate
AetherPluginDestroy
AetherPluginReset
AetherPluginSetDouble
AetherPluginSetString
AetherPluginGetOptionCount
AetherPluginGetOptionInfo
```

## Notes

The original C# OTD filters are not loaded directly by AetherGUI. These are native C++ ports using the Aether plugin ABI.

For users without Visual Studio/Build Tools, include prebuilt DLLs in the GitHub port folders so installation does not require local compilation.
