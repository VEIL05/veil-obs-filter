param([string]$OutputDirectory='',[int]$Width=320,[int]$Height=320,[int]$AsyncSoakFrames=0)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if (-not $OutputDirectory) { $OutputDirectory=Join-Path $projectRoot 'work/veil-background-smoke' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$compiler=Join-Path $projectRoot 'third_party/toolchain/llvm-mingw-20261020-ucrt-x86_64/bin/clang++.exe'
$sdk=Join-Path $projectRoot 'third_party/obs-sdk'
$exe=Join-Path $OutputDirectory 'background_smoke.exe'
& $compiler -std=c++17 -O2 -DNOMINMAX -DWIN32_LEAN_AND_MEAN -Wno-deprecated-declarations -static "-I$sdk/obs-studio-33.0.1/libobs" "-I$sdk/generated" (Join-Path $PSScriptRoot 'background_smoke.cpp') (Join-Path $sdk 'lib/libobs.dll.a') -lole32 -lwindowscodecs -luuid -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Background harness compilation failed' }
$env:PATH='C:\Program Files\obs-studio\bin\64bit;'+$env:PATH
$runArgs=@((Join-Path $projectRoot 'dist/obs-trueitem-beauty'),'C:\Program Files\obs-studio',$OutputDirectory,(Join-Path $projectRoot 'third_party/face-tracking/fixtures/astronaut-320.bgra'),$Width,$Height)
if($AsyncSoakFrames -gt 0){$runArgs+=$AsyncSoakFrames}
& $exe @runArgs 1> (Join-Path $OutputDirectory 'stdout.log') 2> (Join-Path $OutputDirectory 'libobs.log')
$result=$LASTEXITCODE
Get-Content -LiteralPath (Join-Path $OutputDirectory 'stdout.log')
if ($result -ne 0) { Get-Content -LiteralPath (Join-Path $OutputDirectory 'libobs.log') -Tail 30 }
exit $result
