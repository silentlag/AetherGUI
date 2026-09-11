<div align="center">

# AetherGUI Contribution Guidelines

Thanks for your interest in improving AetherGUI! This document covers the
rules for contributing code, plugins, and themes.

## Ground rules

 **Ask before you fork commercially.** The license allows personal use and
  modifications, but redistributing the project (or substantial parts of it)
  under your own name, or borrowing code into another project, requires
  written permission from the author (see LICENSE.md).
 Contributions merged into this repository remain under the project's
  license. By submitting a pull request you agree to that.

## How to contribute

 Fork the repository and create a branch for your change
   (`git checkout -b fix/my-fix`).
 Keep the diff minimal: fix one thing per pull request.
 Test your build locally before opening the PR:
    GUI and service build cleanly in Visual Studio (x64)
    the app starts, connects to the service, and saves/loads its config
 Open a pull request with a short description of **what** changed and
   **why**.

## Code style

 C++17, no external runtime dependencies beyond the Win32/D2D stack that is
  already used.
 Keep comments short and only where the code is not self-explanatory.
 UI text is English; avoid non-ASCII characters in string literals
  (use escapes like `\x2726` where needed).
 Match the file you are editing - do not reformat untouched code.

## Plugins and themes

 Plugin examples live in `plugins-example/`, catalog entries in `Plugins/`.
  A plugin needs: source file, `aether-plugin.json` metadata, and a build
  script (`build_aether_plugin.ps1`).
 Catalog plugins must include full source - no binary-only submissions.
 Themes are drop-in JSON files in `themes/` - follow the key layout of the
  bundled examples.

## Reporting issues

Include: AetherGUI version, Windows version, tablet model, and the log
(console tab or `AetherGUI.log`). Export Diagnostics (Diagnostics tab)
collects most of this for you.

<div align="center">
