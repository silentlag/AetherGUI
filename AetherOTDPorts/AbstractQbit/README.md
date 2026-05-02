# Aether OTD Native Ports

Native C++ ports for testing OpenTabletDriver-style filters in AetherGUI.

Included ports:

- `BezierInterpolatorAether`
- `RadialFollowAether`

These plugins export the Aether native plugin API and can be installed through AetherGUI.

## Build all

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

## Install through AetherGUI

1. Open `AetherGUI.exe`.
2. Go to `Filters` -> `Plugins`.
3. Press `Build Source`.
4. Select either:
   - this folder to build the whole solution, or
   - `BezierInterpolatorAether/`, or
   - `RadialFollowAether/`.
5. AetherGUI builds the project, finds a valid Aether DLL, installs it into `plugins/`, reloads filters, and displays plugin options.
