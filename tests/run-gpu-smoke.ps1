param(
    [string]$ObsRoot = 'C:\Program Files\obs-studio',
    [string]$ModuleRoot = '',
    [string]$OutputDirectory = '',
    [ValidateRange(320,1920)][int]$Width=320,
    [ValidateRange(180,1080)][int]$Height=320,
    [string]$PresetPath = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $ModuleRoot) { $ModuleRoot = Join-Path $projectRoot 'dist\obs-trueitem-beauty' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'work\gpu-smoke' }
$compilerRoot = Join-Path $projectRoot 'third_party\toolchain\llvm-mingw-20261020-ucrt-x86_64'
$sdkRoot = Join-Path $projectRoot 'third_party\obs-sdk'
$compiler = Join-Path $compilerRoot 'bin\clang++.exe'
$executable = Join-Path $OutputDirectory 'gpu_smoke.exe'
$moduleDll = Join-Path $ModuleRoot 'bin\64bit\obs-trueitem-beauty.dll'
$moduleData = Join-Path $ModuleRoot 'data'
$fixture = Join-Path $projectRoot 'third_party\face-tracking\fixtures\astronaut-320.bgra'
if ($PresetPath) {
    $preset = (Resolve-Path -LiteralPath $PresetPath).Path
} else {
    $preset = Join-Path $projectRoot 'presets\trueitem-user-2026-11-03.json'
    if (-not (Test-Path -LiteralPath $preset -PathType Leaf)) {
        $preset = Join-Path $projectRoot 'presets\neutral.json'
    }
}
foreach ($required in @($compiler,$moduleDll,$fixture,$preset,(Join-Path $moduleData 'beauty.effect'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing required test input: $required" }
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
& $compiler -std=c++17 -O2 -Wall -Wextra -Wno-missing-field-initializers -Wno-deprecated-declarations -DNOMINMAX -DWIN32_LEAN_AND_MEAN -static "-I$sdkRoot\obs-studio-33.0.1\libobs" "-I$sdkRoot\generated" (Join-Path $PSScriptRoot 'gpu_smoke.cpp') (Join-Path $sdkRoot 'lib\libobs.dll.a') -lole32 -lwindowscodecs -luuid -o $executable
if ($LASTEXITCODE -ne 0) { throw "GPU harness compilation failed: $LASTEXITCODE" }
$originalPath = $env:PATH
try {
    $env:PATH = (Join-Path $ObsRoot 'bin\64bit') + ';' + (Join-Path $ModuleRoot 'bin\64bit') + ';' + $originalPath
    & $executable $ObsRoot $moduleDll $moduleData $fixture $preset $OutputDirectory $Width $Height 1> (Join-Path $OutputDirectory 'stdout.log') 2> (Join-Path $OutputDirectory 'libobs.log')
    $result = $LASTEXITCODE
    Get-Content -LiteralPath (Join-Path $OutputDirectory 'stdout.log')
    if ($result -ne 0) { Get-Content -LiteralPath (Join-Path $OutputDirectory 'libobs.log') -Tail 55 }
    Write-Output "GPU evidence: $OutputDirectory"
    exit $result
} finally { $env:PATH = $originalPath }
