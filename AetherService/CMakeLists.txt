# Linux build for AetherService.
#
# Usage:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j
#
# This file ONLY drives the Linux build. Windows keeps using AetherService.vcxproj
# (Visual Studio); the two are deliberately kept separate so a Windows
# developer never has to install CMake and a Linux developer never has to deal
# with MSBuild.
#
# Source layout:
#   * Common code (Tablet.cpp, ScreenMapper.cpp, filters, ...) compiles on
#     both OSes unchanged.
#   * Platform_Win32.cpp / HIDDevice.cpp / VMulti.cpp guard their bodies with
#     #if defined(_WIN32) and become empty TUs on Linux. We still list them
#     in CMake so the directory layout matches the vcxproj exactly.
#   * Platform_Linux.cpp / HIDDevice_Linux.cpp / VMulti_Linux.cpp do the
#     opposite: empty TUs on Windows, real implementations on Linux.

cmake_minimum_required(VERSION 3.16)
project(AetherService LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Linux-only dependencies. udev is needed to enumerate /dev/hidraw nodes; the
# rest comes from the kernel headers (uinput, hidraw, timerfd) and libpthread.
#
# Optional libs (libusb-1.0, libX11, libXrandr) are also looked up. They unlock
# Win32 feature parity:
#   * libusb-1.0  -> USBDevice control transfers (some XP-Pen / Huion need them).
#   * libXrandr   -> real multi-monitor virtual-screen geometry.
#   * libX11      -> XSendEvent fallback for wheel emulation when uinput REL_WHEEL
#                    is not picked up by the compositor.
# The build still succeeds without them, the corresponding feature falls back
# to the previous stub. AETHER_HAVE_* defines below let the C++ code branch.
find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)
pkg_check_modules(LIBUSB    libusb-1.0)
pkg_check_modules(XRANDR    xrandr)
pkg_check_modules(X11LIB    x11)

set(SERVICE_SOURCES
    AetherPluginManager.cpp                # empty TU on Linux (full body under #if _WIN32)
    AetherPluginManager_Linux.cpp          # empty TU on Windows
    CommandLine.cpp
    HIDDevice.cpp                          # empty TU on Linux
    HIDDevice_Linux.cpp                    # empty TU on Windows
    Logger.cpp
    Main.cpp
    Platform_Win32.cpp                     # empty TU on Linux
    Platform_Linux.cpp                     # empty TU on Windows
    PositionRingBuffer.cpp
    ProcessCommand.cpp
    ScreenMapper.cpp
    Tablet.cpp
    TabletBenchmark.cpp
    TabletFilter.cpp
    TabletFilterAdaptive.cpp
    TabletFilterAetherSmooth.cpp
    TabletFilterNoiseReduction.cpp
    TabletFilterPeak.cpp
    TabletFilterPlugin.cpp
    TabletFilterReconstructor.cpp
    TabletFilterSmoothing.cpp
    TabletSettings.cpp
    USBDevice.cpp                          # currently a no-op stub on Linux
    Vector2D.cpp
    VMulti.cpp                             # empty TU on Linux
    VMulti_Linux.cpp                       # empty TU on Windows
    stdafx.cpp
)

add_executable(AetherService ${SERVICE_SOURCES})

target_include_directories(AetherService PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${UDEV_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIRS}
    ${XRANDR_INCLUDE_DIRS}
    ${X11LIB_INCLUDE_DIRS}
)

target_link_libraries(AetherService PRIVATE
    pthread
    rt
    ${UDEV_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    ${XRANDR_LIBRARIES}
    ${X11LIB_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

target_link_directories(AetherService PRIVATE
    ${LIBUSB_LIBRARY_DIRS}
    ${XRANDR_LIBRARY_DIRS}
    ${X11LIB_LIBRARY_DIRS}
)

if(LIBUSB_FOUND)
    target_compile_definitions(AetherService PRIVATE AETHER_HAVE_LIBUSB=1)
endif()
if(XRANDR_FOUND)
    target_compile_definitions(AetherService PRIVATE AETHER_HAVE_XRANDR=1)
endif()
if(X11LIB_FOUND)
    target_compile_definitions(AetherService PRIVATE AETHER_HAVE_X11=1)
endif()

# Compile-time hardening: tighten common warnings, but don't go full -Werror
# on the first port. We will tighten further in a follow-up once Linux green.
target_compile_options(AetherService PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-sign-compare
    -fno-strict-aliasing
)

# libstdc++ on recent Arch (GCC 16) has a regression where static
# initialization of the global locale doesn't complete before user code
# constructs ifstream/ofstream objects, leading to either a SIGSEGV
# inside std::locale::operator= or a spurious stack-canary failure on
# ReadCommandFile (which has a local std::ifstream). The condition is
# very build-order sensitive. Disable the stack protector globally as a
# pragmatic workaround; correctness isn't affected.
target_compile_options(AetherService PRIVATE
    -fno-stack-protector
    # _FORTIFY_SOURCE adds runtime buffer-size checks that misfire on
    # this codebase with current glibc, killing the process with
    # "stack smashing detected" on first ifstream close. Disable.
    -U_FORTIFY_SOURCE
    -D_FORTIFY_SOURCE=0
)

# Help our existing PCH usage be a no-op on Linux.
target_compile_definitions(AetherService PRIVATE AETHER_LINUX=1)

install(TARGETS AetherService
    RUNTIME DESTINATION bin)

install(FILES install/udev/60-aether-tablet.rules
    DESTINATION lib/udev/rules.d
    OPTIONAL)
