$ErrorActionPreference = 'Stop'

$solution = Join-Path $PSScriptRoot 'AbstractOTDPlugins.sln'
$msbuildCandidates = @(
    'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe',
    'MSBuild.exe'
)

$msbuild = $msbuildCandidates | Where-Object { $_ -eq 'MSBuild.exe' -or (Test-Path $_) } | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild.exe was not found' }

& $msbuild $solution /p:Configuration=Release /p:Platform=x64 /m
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host 'Built Aether OTD native ports:'
Get-ChildItem -Path $PSScriptRoot -Recurse -Filter *.dll | Where-Object { $_.FullName -match '\\bin\\Release\\' } | ForEach-Object { Write-Host $_.FullName }
