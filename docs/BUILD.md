# Build VEIL 0.4.7 from a checkout

These instructions target Windows x64 and OBS Studio 33.0.1 installed at
`C:\Program Files\obs-studio`. Run PowerShell from the repository root. Paths are
relative to the checkout, so it does not need a particular drive or folder name.

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
