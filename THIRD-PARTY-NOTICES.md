# VEIL third-party notices

This historical VEIL 0.4.5 branch is a collection of separately licensed OBS modules, code adaptations,
models and runtime dependencies. The scope is described in [LICENSE](LICENSE)
and [docs/LICENSING.md](docs/LICENSING.md); third-party rights are unchanged.
For a readable acknowledgement list, see [CREDITS.md](CREDITS.md).
For exact source archives, versions, patches and rebuild inputs, see
[docs/DEPENDENCY-SOURCES.md](docs/DEPENDENCY-SOURCES.md).

## Adopted source

| Upstream | Version / commit | VEIL use | Retained notice |
| --- | --- | --- | --- |
| [ctbot000/face-beautifier](https://github.com/ctbot000/face-beautifier) | `ca5ad40b58e2d0133de6b22282b29000d3667046` | Native port of beauty mathematics, shader regions, original control mappings and One Euro smoothing; VEIL adds native rasterisation, bounded shape/detail and OBS integration | [MIT, Copyright 2026 CTBot000](third_party/face-beautifier/LICENSE) |
| [dinjufen/lumina-beauty](https://github.com/dinjufen/lumina-beauty) | `f1472d581dab9706c8536e0decd46592f9045d44` | CPU detector/478-point landmark decoding and rotated ROI tracking adapted; upstream renderer/segmentation not used | [MIT and scope](third_party/face-tracking/LICENSE-LUMINA.txt), [adaptation notice](third_party/face-tracking/NOTICE.md) |
| [FiniteSingularity/obs-advanced-masks](https://github.com/FiniteSingularity/obs-advanced-masks) | `v1.6.1` | Three unchanged C components and original shaders in separate VEIL finishing helper; full original DLL retained for independent old filters | [GPL version 2](third_party/background-engines/advanced/UPSTREAM-LICENSE.txt), [file hashes](third_party/background-engines/advanced-components/PROVENANCE.json) |

The finishing helper does not include the full module's Qt/SVG/Font Awesome or
network integration. That statement describes the helper only; the retained
complete upstream DLL remains governed by all its original component notices.

## Models and runtime binaries

| Component | Version / asset | Use and license |
| --- | --- | --- |
| [Google MediaPipe](https://github.com/google-ai-edge/mediapipe), converted via pinned Lumina | `face_detector.onnx`, `face_landmarks.onnx` | Apache-2.0, only these two face assets shipped. [Model notice](models/NOTICE.md), [full license](models/LICENSE-Apache-2.0.txt), [hashes](models/PROVENANCE.json) |
| [PeterL1n/RobustVideoMatting](https://github.com/PeterL1n/RobustVideoMatting) | `v1.0.0`, MobileNetV3 FP32 ONNX | Active automatic person-removal model, unchanged. [GPL version 3](third_party/video-matting/data/licenses/RVM-GPL-3.0.txt) |
| [PeterL1n/BackgroundMattingV2](https://github.com/PeterL1n/BackgroundMattingV2) | `v1.0.0`, MobileNetV2 HD ONNX | Historical adapter/model; inactive in normal product but asset remains included. [MIT, University of Washington 2020](third_party/video-matting/data/licenses/BMv2-MIT.txt) |
| [kaito-tokyo/live-backgroundremoval-lite](https://github.com/kaito-tokyo/live-backgroundremoval-lite) | `4.0.1` | Unchanged official Windows DLL retained for old scenes. [GPL-3.0-or-later](third_party/background-engines/lite/UPSTREAM-LICENSE.txt); shader/locale annotations have separate Apache-2.0 terms in [REUSE.toml](third_party/background-engines/lite/data/REUSE.toml). Only `main.effect` has the documented [alpha patch](third_party/background-engines/lite-premultiplied-alpha.patch) |
| [Microsoft ONNX Runtime](https://github.com/microsoft/onnxruntime) | CPU `1.31.1`, matting DirectML package `1.24.4` | MIT, original binaries and notices retained. [CPU license](third_party/onnxruntime/LICENSE), [CPU notices](third_party/onnxruntime/ThirdPartyNotices.txt), [matting license](third_party/video-matting/data/licenses/ONNX-Runtime-LICENSE.txt), [matting notices](third_party/video-matting/data/licenses/ONNX-Runtime-ThirdPartyNotices.txt). Matting DLL is renamed without changing its contents |
| Microsoft DirectML | `Microsoft.AI.DirectML 1.15.4`, Windows x64 DLL | Separately licensed Microsoft software, **not open source**. [Microsoft terms](third_party/video-matting/data/licenses/DirectML-LICENSE.txt) and [third-party notices](third_party/video-matting/data/licenses/DirectML-ThirdPartyNotices.txt) retained. Windows/Xbox use and redistribution scope remain subject to those terms |

Pinned release and file hashes are in the [engine provenance](third_party/background-engines/PROVENANCE.json)
and [matting provenance](third_party/video-matting/data/PROVENANCE.json).
The Lite package's ncnn, CURL, fmt, wolfSSL, zlib and other supplied notices are
retained in [its licenses directory](third_party/background-engines/lite/data/licenses).
Their presence does not establish that every library is linked in every platform build.

## SDK and compiler runtimes

VEIL links to the user's installed libobs. OBS Studio 33.0.1 SDK headers/import
materials retain [OBS's GPL-2.0-or-later copying text](third_party/obs-sdk/obs-studio-33.0.1/COPYING).
The OBS application itself is not bundled.

LLVM-MinGW `20261020` UCRT x86_64 is a development prerequisite. Its libc++, unwind,
MinGW and winpthreads runtimes are statically linked by the build. CMake copies
their original license/exception notices from the pinned toolchain into packaged
`data/licenses/`. [Build instructions](docs/BUILD.md) and the
[bootstrap script](scripts/build/bootstrap_tools.py) pin the toolchain source/release.

## Reference-only work and test fixture

[PyMatting](https://github.com/pymatting/pymatting)'s Shared Matting colour-line
component is a mathematical reference for the independently implemented bounded
projection in the retired reference-background feature. Its Python/Numba runtime
and complete algorithm are not embedded. The [MIT notice](third_party/background-engines/PyMatting-MIT.txt)
is retained as acknowledgement. BGOBS was used only for historical research and
model-hash comparison; its wrapper, phone/USB/ADB code and updater are not bundled.

The public NASA astronaut fixture comes from scikit-image `v0.21.0`; its original
image and resized BGRA test derivative are development-only, not installed.
[Fixture source, public-domain notice and hashes](third_party/face-tracking/NOTICE.md#public-test-fixture)
are retained. No user camera/background image or OBS account data is part of this repository.

## Current and archived features

This branch preserves captured-background registration, storage and preview.
The current product on main retires that mode and exposes automatic person
removal and chroma key. This historical branch is not the current installer target.
