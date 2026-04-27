# AetherGUI

AetherGUI is a lightweight Windows tablet driver focused on low-latency cursor output, clean configuration, and a modern native GUI. It is built around a small service process that reads tablet HID/USB reports and a Direct2D-based control panel for area mapping, filters, output modes, and performance tuning.

This project is based on a classic two-process tablet driver architecture and includes custom fixes and experiments for modern Windows input behavior, high refresh-rate play, and older Wacom tablets.

## Highlights

- Native Windows GUI with tablet area preview, live cursor visualization, profiles, themes, and filter controls.
- Multiple output modes: Absolute, Relative, and Windows Ink/Digitizer through VMulti when available.
- High polling / overclock mode with sub-millisecond scheduling for targets up to 2000 Hz.
- Smoothing, antichatter, noise reduction, adaptive prediction, reconstructor, and Aether Smooth filter paths.
- Embedded tablet configuration database, so the service can start even when external config files are missing.
- Improved handling for relative mode edge cases, invalid-position resets, and raw input event coalescing.

## Project Layout

```text
AetherGUI/        Native control panel and renderer
AetherService/    Tablet reader, filters, mappers, and output backend
AetherGUI.sln     Visual Studio solution
```

## Requirements

- Windows 10 or Windows 11
- Visual Studio 2022 or newer with C++ desktop development tools
- Windows SDK
- Optional: VMulti driver for Windows Ink/Digitizer output

## Building

Open `AetherGUI.sln` in Visual Studio and build:

```text
Configuration: Release
Platform: x64
```

The service binary is produced at:

```text
x64/Release/AetherService.exe
```

The GUI binary is produced by the Visual Studio project output settings.

## Usage

1. Build the solution or use a packaged release.
2. Start `AetherGUI.exe`.
3. Select the output mode and tablet area.
4. Tune filters and overclock settings.
5. Restart the driver from the GUI after changing low-level service settings.

For best results with games that use raw input, test both Absolute and Relative modes and keep the overclock value at the highest rate your system can handle without frame-time spikes.

## Notes

- Some Wacom tablets expose different HID interfaces depending on whether official Wacom drivers are installed.
- If a tablet is not detected, check the GUI console for the `Tablet found!` line and HID warnings.
- Very high output rates can increase CPU scheduling pressure. If a game starts stuttering, try 1000 Hz or 1500 Hz before using 2000 Hz.

## Credits

- Inspired by classic low-latency tablet driver projects.
- Device configuration ideas and tablet metadata were cross-checked against OpenTabletDriver.
