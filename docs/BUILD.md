# Build VEIL 0.4.7 from a checkout

These instructions target Windows x64 and OBS Studio 33.0.1 installed at
`C:\Program Files\obs-studio`. Run PowerShell from the repository root. Paths are
relative to the checkout, so it does not need a particular drive or folder name.
An official OBS 33.0.1 portable ZIP extracted to an owned development directory
can supply the runtime instead. This does not install OBS or read a user profile.
Keep the existing installed OBS and its settings separate from build outputs.

## Prerequisites

- OBS Studio 33.0.1 x64 in the location above.
- Python 3.12 or later, available as `python`.
- Internet access for the first toolchain download and Python package install.
- The Windows .NET Framework C# compiler for building the installer. The installer
  script checks the standard Framework64 compiler location before compiling.

The checkout vendors the reviewed ONNX Runtime and DirectML runtimes, face and
matting models, compatibility modules, OBS header subset and import library.
It excludes the LLVM toolchain, Python environment, downloaded archives and
build outputs. No global PATH or OBS settings changes are needed.

## Prepare the local tools

```powershell
python ./scripts/build/bootstrap_tools.py
python -m venv ./third_party/toolchain/python-tools
./third_party/toolchain/python-tools/Scripts/python.exe -m pip install -r ./requirements-build.txt
./third_party/toolchain/python-tools/Scripts/python.exe ./scripts/build/prepare_sdk.py
```

The bootstrap fetches LLVM-MinGW **20261020 UCRT x86_64** and the public OBS
**33.0.1** source archive into project-local directories. Both downloads must match
the pinned SHA-256 values in the script. The toolchain digest was obtained from
the official release asset; the OBS source digest is the recorded archive digest.
A digest mismatch stops preparation before extraction.

`prepare_sdk.py` reads the installed x64 `obs.dll` exports and regenerates the
import library and small generated header inside `third_party/obs-sdk`. It writes
a local provenance record under `work/build-tools`; it does not modify OBS.
If OBS or LLVM-MinGW is absent, the script reports the missing path.

For an extracted portable runtime, pass the same root to SDK preparation, build
and both rendering checks. Obtain the x64 ZIP from the official
[OBS 33.0.1 release](https://github.com/obsproject/obs-studio/releases/tag/33.0.1)
and verify its release SHA-256 before extraction. The commands below use the
directory containing `bin/64bit/obs.dll`; they do not start the OBS application.

```powershell
$obsRoot = '<owned directory>/obs-33.0.1'
./third_party/toolchain/python-tools/Scripts/python.exe ./scripts/build/prepare_sdk.py --obs-root $obsRoot
./build.ps1 -ObsRoot $obsRoot
./tests/run-gpu-smoke.ps1 -ObsRoot $obsRoot -PresetPath ./presets/neutral.json -Width 1280 -Height 720 -OutputDirectory ./work/gpu-check
./tests/run-background-smoke.ps1 -ObsRoot $obsRoot -Width 1280 -Height 720 -OutputDirectory ./work/background-check
```

The independent source repository remains
[VEIL05/veil-obs-filter](https://github.com/VEIL05/veil-obs-filter).
Use an owned issue checkout for changes and a fresh destination for each release
candidate. A local candidate, a published release and an installed plugin are
separate results; building a candidate does not update either of the latter.

The scripts locate the checkout from their own paths. They can be invoked from
another working directory by using the full script path. Download caches and
preparation records stay under `work/build-tools`.

## Build and check

```powershell
./build.ps1
```

The build uses the local Clang, CMake and Ninja, runs CTest, and packages the
plugin under `dist/obs-trueitem-beauty`. Normal builds keep the archived
captured-background feature disabled. Nothing is installed into OBS at this step.

The repository's default preset is `presets/neutral.json`. When an existing local
personal preset is present, the build preserves its established packaging
behaviour. Personal preset data should stay outside Git.

For the independent libobs rendering checks:

```powershell
./tests/run-gpu-smoke.ps1 -PresetPath ./presets/neutral.json -Width 1280 -Height 720 -OutputDirectory ./work/gpu-check
./tests/run-background-smoke.ps1 -Width 1280 -Height 720 -OutputDirectory ./work/background-check
```

The GPU check accepts `-PresetPath` for an explicit preset. Without that argument,
it prefers the existing local personal preset and falls back to the neutral
preset when the personal file is absent. Both smoke checks use a public fixture
rather than a live camera or saved OBS profile. Build and fixture results do not
prove live-camera quality, capture-device reliability or broadcast stability.

## Build the installer

```powershell
./installer/build-installer.ps1
```

The output is `installer/out/VEIL-OBS-Filter-Setup.exe`. This command builds the
installer; installation is a separate action. Its diagnostic self-test can use a
private destination under the checkout:

```powershell
./installer/out/VEIL-OBS-Filter-Setup.exe --self-test "$PWD\work\installer-self-test"
```

Close OBS before installing or updating. The installer protects files in use.

## Public release builds

Build a public installer from a clean checkout of the release tag. Do not copy an
existing personal build or local delivery ZIP into a public release. A clean
checkout has no ignored personal preset, so CMake packages `presets/neutral.json`.
Before packaging, verify that `dist/obs-trueitem-beauty/data/user-preset.json`
matches that neutral file byte for byte. Retain the complete compatibility payload
and license files; the installer and old OBS scenes rely on those modules.

Publish `VEIL-OBS-Filter-Setup.exe`, a source ZIP created with `git archive` from
the same release tag, and `SHA256SUMS.txt` together. The source ZIP includes the
pinned upstream source archives and dependency-source index. The public release
does not include local presets, reference photos, OBS profiles, logs or toolchains.

## Dependency sources

Versions, upstream URLs, hashes and licenses are recorded with the vendored
assets:

| Component | Source record |
| --- | --- |
| LLVM-MinGW and OBS SDK preparation | [bootstrap script](../scripts/build/bootstrap_tools.py) |
| Face models | [model provenance](../models/PROVENANCE.json) and [model notice](../models/NOTICE.md) |
| CPU ONNX Runtime | [runtime provenance](../third_party/onnxruntime/PROVENANCE.json) |
| Matting runtime, DirectML, RVM and retained BMv2 model | [matting provenance](../third_party/video-matting/data/PROVENANCE.json) |
| Lite and Advanced Masks compatibility components | [engine provenance](../third_party/background-engines/PROVENANCE.json) |
| Face-tracking adaptation and public test fixture | [tracking notice](../third_party/face-tracking/NOTICE.md) |

For an upstream refresh, download from the recorded official source, verify the
recorded version and hash, retain notices, and run the build and relevant fixture
checks. The compatibility engine records include exact tagged source archive
URLs. The matting models come from the upstream RVM and BackgroundMattingV2
v1.0.0 releases; their model digests were recorded locally because those legacy
model releases do not supply vendor digests.

Pinned Lite, Advanced Masks, RVM and BMv2 source ZIPs are tracked with their
recorded hashes. See [dependency sources](DEPENDENCY-SOURCES.md) for the exact
archives, integration patches, additional dependency registries, original model
checkpoint/exporter references and rebuild boundaries. [LICENSING.md](LICENSING.md)
explains module and asset scopes, including DirectML's separate terms.

## Current PR merge-ref validation

After committing and pushing the owned issue branch and opening its PR, provide
`VEIL_OBS_ROOT` in this process for the verified OBS 33.0.1 runtime, then run:

```powershell
$env:VEIL_OBS_ROOT = '<verified OBS 33.0.1 root>'
pwsh.exe -NoProfile -File ./scripts/Test-PrValidationGate.ps1 -Root $PWD -MaxTotalSeconds 600 -ChildTimeoutSeconds 120
```

The gate checks the current PR head, canonical main, both fetched merge-ref
parents and exact HEAD/merge tree equality before running the normal build,
CTest, neutral-preset check, installer build and isolated `--self-test`. It checks
source and PR identities again afterwards. A mismatch requires normal additive
integration and another validation. Its only Git write is fetching this exact
PR merge ref into FETCH_HEAD; it never switches branches, stages files or creates
worktrees. Tracked and untracked source changes reject the run. The official
bootstrap expansion under `third_party/obs-sdk/obs-studio-33.0.1/` is the one
explicit untracked exception. The existing bootstrap's `--verify-sdk-only` mode
checks the pinned archive SHA, exact SDK file names and every file's bytes,
without downloading or extracting anything. The gate compares a name/SHA
fingerprint before and after the build and reports the untracked file count,
without claiming a completely clean checkout or deleting dependency sources.
Run outputs stay under ignored `work/pr-validation` and existing build outputs.
No interactive installer, OBS application or user profile is opened. A passing
gate does not publish a release or update an installed plugin.
