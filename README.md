# VEIL · OBS Filter 0.4.7

VEIL is a native Windows x64 OBS filter for face detail, skin, makeup, colour and
background removal. It processes an existing OBS source locally: no browser,
second camera connection or image upload is required.

## Use VEIL

Run `VEIL-OBS-Filter-Setup.exe`, then open a camera source's **Filters** in OBS and
add **VEIL 필터 · 얼굴·피부·메이크업**. Choose a settings section to adjust its controls.

Use **배경 필터 추가** in the background section to add missing background filters.
Each filter has its own controls:

| Filter | Purpose |
| --- | --- |
| VEIL 배경 · 인물 | Remove an ordinary background and reduce outline flicker. |
| VEIL 배경 · 가장자리 | Soften the outline. |
| VEIL 배경 · 크로마키 | Remove a green screen or another solid-colour background. |

Enable the person filter or the chroma-key filter for the background you use.
Background controls live in those filters. Existing filter identifiers and saved
settings remain compatible with earlier VEIL scenes.

## Features

- Face shape and detail, including eye spacing, face length and nose length.
- Skin smoothing and texture, brightness, redness reduction and teeth whitening.
- Lip and blush colours, lip gloss, colour looks, mirror and overall strength.
- Original, split and mask views, built-in presets and JSON import/export.
- Automatic person removal with GPU acceleration and CPU fallback, plus separate
  outline smoothing and solid-colour keying.
- Local OBS settings persistence and a Windows installer with update, rollback
  and protection against replacing files while OBS is running.

The current product uses automatic person removal. Captured-background removal
is retained as archived development work and is disabled in the normal build;
see [the feature boundary](features/registered-background/README.md).

## Build

The supported build target is **Windows x64 with OBS Studio 33.0.1** installed in
its normal location. The source checkout includes pinned runtime dependencies,
models, OBS headers and their notices. A neutral preset is supplied; personal
presets and camera images are not part of the repository.

See [Build from a checkout](docs/BUILD.md) for prerequisites and the full sequence.
After preparing the local toolchain:

```powershell
./build.ps1
./tests/run-gpu-smoke.ps1 -PresetPath ./presets/neutral.json -Width 1280 -Height 720 -OutputDirectory ./work/gpu-check
./tests/run-background-smoke.ps1 -Width 1280 -Height 720 -OutputDirectory ./work/background-check
./installer/build-installer.ps1
```

`build.ps1` compiles, runs CTest and creates `dist/obs-trueitem-beauty`; it does not
install into OBS. The installer is written to
`installer/out/VEIL-OBS-Filter-Setup.exe`.

The smoke checks run an independent libobs process with a public NASA image.
They do not open a camera or load an OBS profile. Passing them verifies the
fixture rendering path; it does not establish live-camera, encoding or long-run
stability on every computer. Other operating systems and OBS versions are not
verified targets.

## Licenses and dependencies

VEIL beauty code is GPL-2.0-or-later; the video adapter is GPL-3.0-or-later.
Third-party components retain their own licenses, including MIT, Apache-2.0,
GPL and Microsoft DirectML redistribution terms. Keep the supplied license and
notice files with redistributed components.

See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md), the dependency provenance
files, and [dependency source references](docs/BUILD.md#dependency-sources).
