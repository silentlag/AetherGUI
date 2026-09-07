# Build Release x64 and stage a distributable folder + zip.
# Usage: powershell -ExecutionPolicy Bypass -File scripts\package.ps1 [-NoZip]
param([switch]$NoZip)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$vh = Get-Content (Join-Path $root 'AetherGUI\Version.h') -Raw
if ($vh -notmatch 'AETHERGUI_VERSION\s+"([^"]+)"') { throw 'Version.h parse failed' }
$version = $Matches[1]
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw 'vswhere not found - install VS Build Tools' }
$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild not found' }

& $msbuild (Join-Path $root 'AetherGUI.sln') /p:Configuration=Release /p:Platform=x64 /m /v:m
if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
$guiExe   = Join-Path $root 'AetherGUI\Release\AetherGUI.exe'
$svcExe   = Join-Path $root 'bin\Release\AetherService.exe'
foreach ($f in @($guiExe, $svcExe)) { if (-not (Test-Path $f)) { throw "missing build output: $f" } }

$out = Join-Path $root "package\AetherGUI-$version"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item $out -ItemType Directory -Force | Out-Null

Copy-Item $guiExe $out
Copy-Item $svcExe $out
foreach ($doc in 'README.md','Release.md','PLUGIN_API.md') {
    $p = Join-Path $root $doc
    if (Test-Path $p) { Copy-Item $p $out }
}
if (Test-Path (Join-Path $root 'themes')) {
    New-Item (Join-Path $out 'themes') -ItemType Directory -Force | Out-Null
    Copy-Item (Join-Path $root 'themes\*.json') (Join-Path $out 'themes')
}

if (-not $NoZip) {
    $zip = "$out.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path "$out\*" -DestinationPath $zip
    Write-Host "packaged: $zip"
} else {
    Write-Host "packaged: $out"
}
