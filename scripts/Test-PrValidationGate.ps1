#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot),
    [ValidateRange(1, 86400)][int]$MaxTotalSeconds = 600,
    [ValidateRange(1, 86400)][int]$ChildTimeoutSeconds = 90
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$timer = [Diagnostics.Stopwatch]::StartNew()
$expectedRepo = 'ttaem00/CVA-VEIL-OBS-Filter'
$allowedBootstrap = 'third_party/obs-sdk/obs-studio-33.0.1/'
$sourceRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('/','\')
$Root = [IO.Path]::GetFullPath($Root).TrimEnd('/','\')
if ($Root -ne $sourceRoot) { throw 'Root must be this script checkout; no other checkout is modified.' }
$git = (Get-Command git -ErrorAction Stop).Source
$gh = (Get-Command gh -ErrorAction Stop).Source
$pwsh = (Get-Command pwsh.exe -ErrorAction Stop).Source

function Invoke-Step([string]$Name, [string]$File, [string[]]$Arguments, [switch]$Quiet) {
    $remaining = $MaxTotalSeconds - $timer.Elapsed.TotalSeconds
    if ($remaining -le 0) { throw "Total validation timeout before: $Name" }
    Write-Host "[START] $Name"
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $File
    $info.WorkingDirectory = $Root
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $info
    try {
        if (-not $process.Start()) { throw "Could not start: $Name" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $milliseconds = [int]([Math]::Min($remaining, $ChildTimeoutSeconds) * 1000)
        if (-not $process.WaitForExit($milliseconds)) {
            $process.Kill($true)
            $process.WaitForExit()
            throw "Child validation timeout: $Name"
        }
        $out = $stdout.GetAwaiter().GetResult()
        $err = $stderr.GetAwaiter().GetResult()
        if (-not $Quiet -or $process.ExitCode -ne 0) {
            if ($out) { Write-Host $out.TrimEnd() }
            if ($err) { Write-Host $err.TrimEnd() }
        }
        if ($process.ExitCode -ne 0) { throw "$Name failed (exit $($process.ExitCode))." }
        Write-Host "[PASS] $Name"
        return $out
    } finally { $process.Dispose() }
}
function Git-Read([string]$Name, [string[]]$Arguments) {
    return (Invoke-Step $Name $git (@('--no-optional-locks', '-c', 'core.quotepath=false') + $Arguments) -Quiet).Trim()
}
function Assert-SourceClean {
    foreach ($arguments in @(@('diff','--name-only','--'), @('diff','--cached','--name-only','--'))) {
        $changed = Git-Read 'tracked source clean' $arguments
        if ($changed) { throw "Tracked changes must be committed before PR validation: $changed" }
    }
    $untracked = (Git-Read 'untracked source inventory' @('ls-files','--others','--exclude-standard','-z')).Split([char]0, [StringSplitOptions]::RemoveEmptyEntries)
    $bootstrap = @($untracked | Where-Object { $allowedBootstrap -and $_.StartsWith($allowedBootstrap, [StringComparison]::Ordinal) })
    $other = @($untracked | Where-Object { -not ($allowedBootstrap -and $_.StartsWith($allowedBootstrap, [StringComparison]::Ordinal)) })
    if ($other.Count) { throw "Untracked source is not validated: $(($other | Select-Object -First 10) -join ', ')" }
    Write-Host "[PASS] Source inventory: tracked clean; approved bootstrap untracked files=$($bootstrap.Count)"
    return $bootstrap.Count
}
function Read-Pr {
    $json = Invoke-Step 'current PR identity' $gh @('pr','view',$branch,'--repo',$expectedRepo,'--json','number,state,headRefName,headRefOid,baseRefName,baseRefOid') -Quiet
    $pr = $json | ConvertFrom-Json
    if ($pr.state -ne 'OPEN' -or $pr.headRefName -ne $branch -or $pr.baseRefName -ne 'main') { throw 'Expected the open current-branch PR targeting canonical main.' }
    $baseLine = Git-Read 'canonical main identity' @('ls-remote','--exit-code','origin','refs/heads/main')
    $baseOid = ($baseLine -split '\s+')[0]
    if ($pr.baseRefOid -ne $baseOid) { throw 'PR base differs from current canonical main; integrate additively and retry.' }
    $headLine = Git-Read 'canonical branch identity' @('ls-remote','--exit-code','origin',"refs/heads/$branch")
    if ($pr.headRefOid -ne ($headLine -split '\s+')[0]) { throw 'PR head differs from the current canonical branch.' }
    return $pr
}
try {
    $top = Git-Read 'checkout root' @('rev-parse','--show-toplevel')
    if ([IO.Path]::GetFullPath($top).TrimEnd('/','\') -ne $Root) { throw 'Root is not the Git top-level checkout.' }
    $branch = Git-Read 'checkout branch' @('symbolic-ref','--short','HEAD')
    if ($branch -notmatch '^codex/issue-[0-9]+-') { throw 'Use the owned issue branch; main and detached HEAD are not validation targets.' }
    $remote = Git-Read 'repository remote' @('remote','get-url','origin')
    if ($remote -notmatch ('^(https://github\.com/|git@github\.com:)' + [regex]::Escape($expectedRepo) + '(\.git)?$')) { throw 'Origin does not match this product repository.' }
    $bootstrapBefore = Assert-SourceClean
    $sdkPython = Join-Path $Root 'third_party/toolchain/python-tools/Scripts/python.exe'
    $sdkVerifier = Join-Path $Root 'scripts/build/bootstrap_tools.py'
    $sdkBefore = (Invoke-Step 'pinned SDK source bytes' $sdkPython @($sdkVerifier,'--verify-sdk-only') -Quiet) | ConvertFrom-Json
    $head = Git-Read 'local source identity' @('rev-parse','HEAD')
    $pr = Read-Pr
    if ($head -ne $pr.headRefOid) { throw 'Local HEAD differs from PR head; push or integrate normally first.' }
    # This exact fetch updates FETCH_HEAD only, never a branch, index, or worktree.
    $null = Invoke-Step 'fetch exact PR merge ref' $git @('fetch','--no-tags','--no-prune','--no-recurse-submodules','--no-auto-maintenance','origin',"refs/pull/$($pr.number)/merge")
    $merge = Git-Read 'PR merge identity' @('rev-parse','FETCH_HEAD')
    $parents = (Git-Read 'PR merge parents' @('rev-list','--parents','-n','1',$merge)) -split ' '
    if ($parents.Count -ne 3 -or $parents[1] -ne $pr.baseRefOid -or $parents[2] -ne $head) { throw 'Merge-ref parents differ from current base/head; integrate additively and retry.' }
    $tree = Git-Read 'source tree' @('rev-parse',"$head`^{tree}")
    if ($tree -ne (Git-Read 'merge tree' @('rev-parse',"$merge`^{tree}"))) { throw 'Local source tree differs from merge tree; integrate additively and retry.' }
    Write-Host "[PASS] Exact merge-tree equivalence: repo=$expectedRepo PR=$($pr.number) head=$head base=$($pr.baseRefOid) merge=$merge tree=$tree"
    if (-not $env:VEIL_OBS_ROOT -or -not (Test-Path -LiteralPath (Join-Path $env:VEIL_OBS_ROOT 'bin/64bit/obs.dll') -PathType Leaf)) { throw 'Provide VEIL_OBS_ROOT for the verified OBS 33.0.1 runtime.' }
    if (Test-Path -LiteralPath (Join-Path $Root 'presets/trueitem-user-2026-11-03.json')) { throw 'Use a source checkout without the ignored personal preset; it is not a PR validation input.' }
    $null = Invoke-Step 'build and CTest' $pwsh @('-NoProfile','-File',(Join-Path $Root 'build.ps1'),'-ObsRoot',$env:VEIL_OBS_ROOT)
    $neutral = (Get-FileHash -LiteralPath (Join-Path $Root 'presets/neutral.json') -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath (Join-Path $Root 'dist/obs-trueitem-beauty/data/user-preset.json') -Algorithm SHA256).Hash -ne $neutral) { throw 'Packaged preset differs from the neutral source input.' }
    # The .NET Framework installer's nested self-test must stay below MAX_PATH.
    # The caller can supply an already issued, short product-test temp directory.
    $outputRoot = if ($env:VEIL_VALIDATION_TEMP_ROOT) { $env:VEIL_VALIDATION_TEMP_ROOT } else { Join-Path $Root 'work/pr-validation' }
    if ($env:VEIL_VALIDATION_TEMP_ROOT -and -not (Test-Path -LiteralPath $outputRoot -PathType Container)) { throw 'VEIL_VALIDATION_TEMP_ROOT must be an existing owned test directory.' }
    $run = Join-Path $outputRoot ('v-' + [guid]::NewGuid().ToString('N').Substring(0,8))
    if (Test-Path -LiteralPath $run) { throw 'Validation output already exists; use a new owned child.' }
    $null = Invoke-Step 'installer build' $pwsh @('-NoProfile','-File',(Join-Path $Root 'installer/build-installer.ps1'),'-OutputDirectory',(Join-Path $run 'installer'))
    $null = Invoke-Step 'isolated installer self-test' (Join-Path $run 'installer/VEIL-OBS-Filter-Setup.exe') @('--self-test',(Join-Path $run 'st'))
    $bootstrapAfter = Assert-SourceClean
    $sdkAfter = (Invoke-Step 'final pinned SDK source bytes' $sdkPython @($sdkVerifier,'--verify-sdk-only') -Quiet) | ConvertFrom-Json
    if ($sdkAfter.source_fingerprint -ne $sdkBefore.source_fingerprint) { throw 'Pinned SDK source snapshot changed during validation.' }
    if ($bootstrapAfter -ne $bootstrapBefore) { throw 'Bootstrap inventory changed during validation.' }
    if ((Git-Read 'final local head' @('rev-parse','HEAD')) -ne $head -or (Git-Read 'final branch' @('symbolic-ref','--short','HEAD')) -ne $branch) { throw 'Source branch/head changed during validation.' }
    $after = Read-Pr
    if ($after.number -ne $pr.number -or $after.headRefOid -ne $head -or $after.baseRefOid -ne $pr.baseRefOid) { throw 'PR head/base changed during validation; retry the new merge ref.' }
    [ordered]@{state='PASS';repository=$expectedRepo;pr=$pr.number;head=$head;base=$pr.baseRefOid;merge_ref=$merge;tree=$tree;bootstrap_untracked=$bootstrapAfter;elapsed_seconds=[Math]::Round($timer.Elapsed.TotalSeconds,1);actual_install_or_user_profile='not invoked'} | ConvertTo-Json -Compress | Write-Host
    exit 0
} catch {
    Write-Host "[FAIL] PR validation: $($_.Exception.Message)"
    exit 1
}
