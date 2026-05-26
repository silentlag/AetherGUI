# AetherGUI on Linux

Native Linux build of the Aether tablet driver. Same feature set as the
Windows version: low-latency HID + USB tablet input, virtual pen output,
filter chain, plugin system, the full GUI.

The Windows build still lives in `AetherGUI/` and `AetherService/`. This
document covers the Linux side: `AetherGUI_Linux/` (front-end) and the
Linux portions of `AetherService/` (the driver).

---

## What works

**Driver (AetherService):**

- HID tablets via udev + hidraw, with VID/PID/UsagePage/Usage scoring
  and indexed string descriptor matching (libusb).
- USB-WinUSB tablets (older XP-Pen / Huion with vendor control transfers)
  via libusb-1.0.
- Pen output through `/dev/uinput`: virtual mouse and virtual digitizer
  devices, all five output modes (Absolute / Relative / Digitizer /
  SendInput / RawAbsolute).
- Wheel emulation through `REL_WHEEL` on the virtual mouse device.
- Multi-monitor geometry through XRandR (primary + virtual desktop bbox).
- Real-time scheduling via `SCHED_FIFO` + `RLIMIT_RTPRIO` (the Linux
  equivalent of MMCSS Pro Audio on Windows).
- High-resolution timer via `timerfd` + `CLOCK_MONOTONIC` with a spin
  tail. Same overclock behavior as the Windows side.
- Plugin loading from `~/.local/share/AetherGUI/plugins/*.so`.

**GUI (AetherGUI_Linux):**

- Full UI ported from the Windows source: Area / Filters / Settings /
  Console / About tabs, all sliders/toggles/radios.
- All twelve themes plus the custom accent picker and per-slot hex
  editor.
- Live pen position, latency stats, output Hz meter.
- Console with command input, colorized log lines, history.
- Plugin manager UI (loading, configuration, options).
- Tab transitions and the background animation set (orbs, twinkling
  stars, fireflies, snow).

## Known gaps

These are stubbed out and produce no-op behavior. The UI still draws and
the buttons still respond, the action behind them is just disabled.

- **File pickers.** "Save Config", "Load Config", "Install Plugin" don't
  open a file dialog yet. Edit `~/.local/share/AetherGUI/configs/*.cfg`
  by hand for now. GTK FileChooser integration is on the list.
- **Update check.** The release-update banner never triggers; the
  GitHub poll uses Win-only WinHTTP. libcurl backend is a follow-up.
- **VMulti install check.** Doesn't apply on Linux at all -- the
  equivalent path on Linux is `/dev/uinput`, which the service creates
  on its own.

---

## Requirements

- x86_64 Linux. Tested on Ubuntu 24.04, Debian 12, Fedora 40, Arch.
- glibc 2.34 or newer (Ubuntu 22.04+, Debian 12+, Fedora 36+).
- An X11 session, or a Wayland session with XWayland enabled. XRandR
  for multi-monitor.
- systemd-logind (default on every mainstream desktop distro). The udev
  rules use the modern `uaccess` tag, so device permissions follow your
  active login session automatically -- no group membership needed.

Runtime libraries: `libsdl2`, `libgl1`, `libfreetype6`, `libusb-1.0-0`,
`libudev1`, `libxrandr2`, `libx11-6`. All standard, almost certainly
already installed.

---

## Quick start (prebuilt bundle)

If you got a `aether-linux-test.tar.gz`:

```sh
tar xzf aether-linux-test.tar.gz
cd aether-linux-test
sudo ./install.sh    # ONE TIME, sets up udev rules
./run.sh             # AS YOUR REGULAR USER, not root
```

Don't run `run.sh` with `sudo`. The driver doesn't need root -- the udev
rules from `install.sh` give your seat session access to `/dev/uinput`
and `/dev/hidraw*` automatically. Running as root means SDL can't talk
to your Wayland/X session and the GUI will fail with `No available
video device`.

`install.sh` installs the udev rules and loads the `uinput` kernel
module. `run.sh` launches the GUI which auto-spawns the driver.

---

## Building from source

Install dev packages first.

Debian / Ubuntu:

```sh
sudo apt install build-essential cmake pkg-config \
    libsdl2-dev libgl-dev libfreetype-dev \
    libusb-1.0-0-dev libudev-dev libxrandr-dev libx11-dev
```

Fedora:

```sh
sudo dnf install gcc-c++ cmake pkgconf-pkg-config \
    SDL2-devel mesa-libGL-devel freetype-devel \
    libusb1-devel systemd-devel libXrandr-devel libX11-devel
```

Arch:

```sh
sudo pacman -S base-devel cmake sdl2 freetype2 \
    libusb systemd-libs libxrandr libx11
```

Then:

```sh
# 1. The driver.
cmake -S AetherService -B AetherService/build -DCMAKE_BUILD_TYPE=Release
cmake --build AetherService/build -j

# 2. The GUI.
cmake -S AetherGUI_Linux -B AetherGUI_Linux/build -DCMAKE_BUILD_TYPE=Release
cmake --build AetherGUI_Linux/build -j
```

Binaries:

- `AetherService/build/AetherService`
- `AetherGUI_Linux/build/AetherGUI`

If you want to point the GUI at a service binary in a non-standard
location:

```sh
./AetherGUI_Linux/build/AetherGUI --service=/path/to/AetherService
```

---

## Permissions

The driver needs read/write access to two device classes:

- `/dev/hidraw*` -- the tablet itself
- `/dev/uinput`  -- the virtual pen output device the driver creates

The shipped udev rules (generated from the OpenTabletDriver 0.6.7
device database, 249 vendor/product ID pairs covering 406 tablet
models) tag both with `uaccess` and `udev-acl`. systemd-logind picks
up these tags and grants per-session ACLs to whichever user is
logged in at the seat. No group membership, no logout, no reboot.

To install:

```sh
sudo cp AetherService/install/udev/60-aether-tablet.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
sudo modprobe uinput
```

If you're on a distro without systemd-logind (rare on desktop, common
on minimal embedded), add yourself to the `input` group:

```sh
sudo usermod -aG input $USER
# log out and back in
```

The rules also set `LIBINPUT_IGNORE_DEVICE` for tablets that libinput
would otherwise grab (most XP-Pen and Huion devices), so the driver
gets exclusive access to the HID stream.

---

## Logs

- GUI session log: `~/.local/share/AetherGUI/AetherGUI.log`
- Service trace: printed to stdout, mirrored into the GUI's Console tab
  and into the same log file.

Both logs include timestamps and a tag (`BRIDGE` for the spawn layer,
`SVC` for the service, `APP` for the GUI). When something goes wrong,
the answer is almost always in there.

---

## Troubleshooting

**Tablet not detected.** Check that the device is enumerated:

```sh
lsusb | grep -iE 'wacom|xp-pen|huion|gaomon|veikk'
```

Then check permissions on the relevant nodes:

```sh
ls -l /dev/uinput /dev/hidraw*
```

`/dev/uinput` should be `crw-rw---- root plugdev`. If it's
`crw------- root root`, the udev rule didn't apply. Re-run
`sudo udevadm control --reload-rules && sudo udevadm trigger`.

**`EACCES` in the log.** Either you skipped `install.sh`, or your
distro doesn't run systemd-logind so the `uaccess` tag isn't doing
anything. Add yourself to the `input` group as a fallback:

```sh
sudo usermod -aG input $USER && newgrp input
```

**`SDL_Init failed: No available video device`.** You ran the GUI as
root. Don't. Use `install.sh` once with sudo, then `./run.sh` as your
regular user. Wayland and X reject connections from a different uid
than the one that owns the session.

**GUI window doesn't open on Wayland.** Install `xwayland` (or run on
X11). SDL falls back to XWayland automatically when the Wayland
backend can't initialize OpenGL.

**Cursor mapping is wrong on multi-monitor.** The service queries
XRandR for the virtual desktop bounding box. If you're on Wayland
without XWayland, XRandR isn't available and the service falls back
to 1920x1080. Either run XWayland or accept the fallback.

**Service crashes on startup.** Open the Console tab; the last lines
tell you why. Common cases:

- Another driver owns the tablet's USB interface. The service tries to
  detach the kernel driver, but if you have a vendor daemon running
  (Wacom's `wacom_kde`, Huion's userspace driver), stop it first.
- The chosen tablet entry needs USB control transfers and libusb
  couldn't claim interface 0. Look for `libusb_claim_interface failed`
  in the log.

---

## Reporting issues

Please include:

- `~/.local/share/AetherGUI/AetherGUI.log` (or at least the last
  ~100 lines around the failure)
- `lsusb -v 2>/dev/null | grep -A 20 -iE 'wacom|xp-pen|huion'`
- `ls -l /dev/uinput /dev/hidraw*`
- `uname -a`
- `cat /etc/os-release`
- Whether you're on X11 or Wayland (`echo $XDG_SESSION_TYPE`)

For visual bugs, a screenshot or a 5-second screen recording helps a
lot more than a description.

---

## How it differs from the Windows build

If you're cross-referencing the Windows source:

- `AetherService/Platform_Linux.cpp` mirrors `Platform_Win32.cpp`. RT
  scheduling, monotonic clock, hi-res timer, dynamic library loading,
  filesystem helpers, monitor info.
- `AetherService/HIDDevice_Linux.cpp` mirrors the Windows
  `HIDDevice.cpp`. Same matching algorithm, sourced from hidraw +
  libudev + libusb instead of SetupAPI / HidP / WinUSB.
- `AetherService/VMulti_Linux.cpp` replaces VMulti's HID-write path
  with `/dev/uinput`. Same five output modes, same coordinate
  conventions.
- `AetherGUI_Linux/src/Renderer.cpp` replaces Direct2D / DirectWrite
  with nanovg over OpenGL 3.2. Same public API as the Windows
  Renderer, so the rest of the GUI didn't have to change.
- `AetherGUI_Linux/src/WinShims.h` and `WinShimsExtra.h` provide the
  Win32 surface (`VK_*`, `OpenClipboard`, `swprintf_s`, virtual-key
  state, file dialogs, etc.) that the unmodified `AetherApp.cpp` and
  `Controls.h` reach for. Stubs return benign values where a real
  implementation isn't there yet.

The Linux GUI uses SDL2 + OpenGL 3.2 + nanovg. The Windows GUI uses
raw Win32 + Direct2D. The application logic on top is the same source
file (`AetherApp.cpp`, 6.2k lines, byte-for-byte identical to the
Windows copy aside from a different `#include` block at the top).
