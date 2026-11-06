param([string]$PayloadDirectory, [string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
if (-not $PayloadDirectory) { $PayloadDirectory = Join-Path $taskRoot 'dist/obs-trueitem-beauty' }
$PayloadDirectory = (Resolve-Path -LiteralPath $PayloadDirectory).Path
$brandIcon = Join-Path $taskRoot 'assets/brand/obs-filter.ico'
$brandImage = Join-Path $taskRoot 'assets/brand/obs-filter.png'
foreach ($brandFile in @($brandIcon, $brandImage)) {
    if (-not (Test-Path -LiteralPath $brandFile -PathType Leaf)) { throw "Missing brand asset: $brandFile" }
}
$workDir = Join-Path $PSScriptRoot 'work'
$outDir = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $PSScriptRoot 'out' }
New-Item -ItemType Directory -Path $workDir, $outDir -Force | Out-Null
foreach ($file in @('bin/64bit/obs-trueitem-beauty.dll', 'bin/64bit/onnxruntime.dll', 'bin/64bit/onnxruntime_providers_shared.dll', 'data/beauty.effect', 'data/models/face_detector.onnx', 'data/models/face_landmarks.onnx', 'data/user-preset.json', 'data/licenses/TRUEITEM-LICENSE.txt', 'data/licenses/THIRD-PARTY-NOTICES.md')) {
    if (-not (Test-Path -LiteralPath (Join-Path $PayloadDirectory $file) -PathType Leaf)) { throw "Missing payload file: $file" }
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = Join-Path $workDir ("payload-" + [guid]::NewGuid().ToString('N') + '.zip')
[IO.Compression.ZipFile]::CreateFromDirectory($PayloadDirectory, $zip, [IO.Compression.CompressionLevel]::Optimal, $false)
$sha = Join-Path $workDir 'payload.sha256'
[IO.File]::WriteAllText($sha, (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant(), [Text.Encoding]::ASCII)
$compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw '.NET Framework C# compiler is unavailable.' }
$output = Join-Path $outDir 'VEIL-OBS-Filter-Setup.exe'
& $compiler /nologo /noconfig /target:winexe /platform:x64 /optimize+ "/out:$output" "/win32icon:$brandIcon" "/resource:$brandIcon,VEIL.Brand.ico" "/resource:$brandImage,VEIL.Brand.png" "/resource:$zip,Trueitem.Payload.zip" "/resource:$sha,Trueitem.Payload.sha256" /reference:System.dll /reference:System.Core.dll /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.IO.Compression.dll /reference:System.IO.Compression.FileSystem.dll (Join-Path $PSScriptRoot 'Installer.cs')
if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed ($LASTEXITCODE)" }
$record = [ordered]@{
    installer = $output
    installerSHA256 = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
    payload = $PayloadDirectory
    payloadSHA256 = [IO.File]::ReadAllText($sha)
    brandIconSHA256 = (Get-FileHash -LiteralPath $brandIcon -Algorithm SHA256).Hash.ToLowerInvariant()
    brandImageSHA256 = (Get-FileHash -LiteralPath $brandImage -Algorithm SHA256).Hash.ToLowerInvariant()
    builtUTC = [DateTime]::UtcNow.ToString('o')
}
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outDir 'build-proof.json') -Encoding UTF8
Write-Host "Single-file installer built: $output"
