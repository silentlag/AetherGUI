$ErrorActionPreference = "Stop"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
	throw "vswhere.exe not found. Install Visual Studio with the 'Desktop development with C++' workload."
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath -or -not (Test-Path $vsPath)) {
	throw "Visual Studio C++ tools not found. Install the 'Desktop development with C++' workload."
}

$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
	throw "vcvars64.bat not found at: $vcvars"
}

$src = Join-Path $PSScriptRoot "moving_average.cpp"
$out = Join-Path $PSScriptRoot "moving_average.dll"

if (Test-Path $out) { Remove-Item $out -Force }
if (Test-Path "$out.exp") { Remove-Item "$out.exp" -Force }
if (Test-Path "$out.lib") { Remove-Item "$out.lib" -Force }
if (Test-Path "$out.obj") { Remove-Item "$out.obj" -Force }

$cmd = "`"$vcvars`" >nul 2>&1 && cl /nologo /O2 /LD /EHsc /utf-8 `"$src`" /Fe:`"$out`" /Fo:`"$PSScriptRoot\`""
cmd /c $cmd
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $out)) {
	throw "Build failed (exit code $LASTEXITCODE)."
}

Write-Host "Built: $out"
exit 0
