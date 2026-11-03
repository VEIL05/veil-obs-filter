param(
    [switch]$SkipTests,
    [ValidateSet('Release', 'RelWithDebInfo', 'Debug')][string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$cmake = Join-Path $taskRoot 'third_party/toolchain/python-tools/Scripts/cmake.exe'
$ctest = Join-Path $taskRoot 'third_party/toolchain/python-tools/Scripts/ctest.exe'
$ninja = Join-Path $taskRoot 'third_party/toolchain/python-tools/Scripts/ninja.exe'
$buildDir = Join-Path $taskRoot "work/build-$Configuration"
$distDir = Join-Path $taskRoot 'dist/obs-trueitem-beauty'

function Invoke-BuildTool {
    param([string]$Executable, [string[]]$ToolArguments)
    & $Executable @ToolArguments
    if ($LASTEXITCODE -ne 0) { throw "Build command failed ($LASTEXITCODE): $Executable" }
}

foreach ($required in @($cmake, $ninja, (Join-Path $taskRoot 'third_party/obs-sdk/lib/libobs.dll.a'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing build dependency: $required" }
}
Invoke-BuildTool $cmake @('-S', $taskRoot, '-B', $buildDir, '-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$ninja", "-DCMAKE_TOOLCHAIN_FILE=$taskRoot/toolchain-llvm-mingw.cmake", "-DCMAKE_BUILD_TYPE=$Configuration", '-DBUILD_TESTING=ON', '-DVEIL_REGISTERED_BACKGROUND=OFF')
Invoke-BuildTool $cmake @('--build', $buildDir, '--parallel', '4')
if (-not $SkipTests) {
    Invoke-BuildTool $ctest @('--test-dir', $buildDir, '--output-on-failure')
}
Invoke-BuildTool $cmake @('--install', $buildDir, '--prefix', $distDir)
Write-Host "Built native OBS plugin: $distDir"
Write-Host 'This command packages files only. It does not modify OBS or start a broadcast.'
