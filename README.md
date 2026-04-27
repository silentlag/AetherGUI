<div align="center">

<h1>AetherGUI</h1>

<p>
  <strong>Low-latency Windows tablet driver with a native control panel</strong>
  <br/>
  Native GUI · HID / Raw Input · Wacom support · High polling output · Adaptive filtering
</p>

<p>
  <img alt="Windows 10 / 11" src="https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?style=for-the-badge&logo=windows&logoColor=white"/>
  <img alt="C++" src="https://img.shields.io/badge/C%2B%2B-Native-00599C?style=for-the-badge&logo=cplusplus&logoColor=white"/>
  <img alt="Visual Studio" src="https://img.shields.io/badge/Visual%20Studio-2022-5C2D91?style=for-the-badge&logo=visualstudio&logoColor=white"/>
  <img alt="Polling" src="https://img.shields.io/badge/Polling-up%20to%202000%20Hz-f97316?style=for-the-badge"/>
</p>

<p>
  AetherGUI pairs a small tablet service with a Direct2D-based interface for area mapping,
  output modes, smoothing filters, profiles, and performance tuning.
</p>

<p align="center">
  <img src="assets/screenshots/main.png" width="780" alt="AetherGUI main window"/>
</p>

<br/>

<h2>· Highlights ·</h2>

<table align="center">
  <tr>
    <td align="center"><strong>Native Control Panel</strong><br/>Area preview · live cursor view · profiles · themes</td>
    <td align="center"><strong>Multiple Output Modes</strong><br/>Absolute · Relative · Windows Ink / Digitizer through VMulti</td>
  </tr>
  <tr>
    <td align="center"><strong>High Polling Output</strong><br/>Sub-millisecond scheduling with targets up to 2000 Hz</td>
    <td align="center"><strong>Advanced Filters</strong><br/>Smoothing · antichatter · noise reduction · adaptive prediction · Aether Smooth</td>
  </tr>
  <tr>
    <td align="center"><strong>Embedded Tablet Configs</strong><br/>Fallback device database when external configs are missing</td>
    <td align="center"><strong>Raw Input Fixes</strong><br/>Relative-mode resets · invalid-position handling · reduced event coalescing</td>
  </tr>
</table>

<br/>

<h2>· Project Layout ·</h2>

<table align="center">
  <tr>
    <th align="center">Path</th>
    <th align="center">Purpose</th>
  </tr>
  <tr>
    <td align="center"><code>AetherGUI/</code></td>
    <td align="center">Native Direct2D control panel and renderer</td>
  </tr>
  <tr>
    <td align="center"><code>AetherService/</code></td>
    <td align="center">Tablet reader, filters, mapper, and output backend</td>
  </tr>
  <tr>
    <td align="center"><code>AetherGUI.sln</code></td>
    <td align="center">Visual Studio solution</td>
  </tr>
</table>

<br/>

<h2>· Requirements ·</h2>

<p>
  Windows 10 / 11 · Visual Studio 2022 or newer · Windows SDK
  <br/>
  Optional: VMulti driver for Windows Ink / Digitizer output
</p>

<br/>

<h2>· Building ·</h2>

<p>
  Open <code>AetherGUI.sln</code> in Visual Studio and build:
</p>

<table align="center">
  <tr>
    <td align="center"><strong>Configuration</strong><br/><code>Release</code></td>
    <td align="center"><strong>Platform</strong><br/><code>x64</code></td>
  </tr>
</table>

<p>
  Service output: <code>x64/Release/AetherService.exe</code>
</p>

<br/>

<h2>· Usage ·</h2>

<p>
  Build the solution · Start <code>AetherGUI.exe</code> · Select output mode and tablet area
  <br/>
  Tune filters and overclock settings · Restart the driver from the GUI when low-level settings change
</p>

<p>
  For games that use raw input, test both Absolute and Relative modes.
  Keep the overclock value at the highest rate your system can handle without frame-time spikes.
</p>

<br/>

<h2>· Notes ·</h2>

<p>
  Some Wacom tablets expose different HID interfaces depending on whether official Wacom drivers are installed.
  <br/>
  If detection fails, check the GUI console for <code>Tablet found!</code> and HID warning lines.
  <br/>
  If 2000 Hz causes stutter, try 1500 Hz or 1000 Hz for a steadier frame time.
</p>

<br/>

<h2>· Credits ·</h2>

<p>
  Inspired by Devocub Tablet Driver.
  <br/>
  Device configuration ideas and tablet metadata were cross-checked against OpenTabletDriver.
</p>

</div>
