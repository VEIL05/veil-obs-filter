# Pinned dependency sources and changes

The VEIL sources, CMake/build scripts, installer source and integration patches
are in this repository. Matching upstream source snapshots are also tracked;
they are not downloaded from an unversioned latest link.

| Component | Pinned source | Local source archive | SHA-256 |
| --- | --- | --- | --- |
| Live Background Removal Lite | [4.0.1](https://github.com/kaito-tokyo/live-backgroundremoval-lite/tree/4.0.1) | [lite-source.zip](../third_party/background-engines/lite-source.zip) | `d6da69e58e90ea7a14810444f6dddcca73d2fd8ee59bb71943e1a12a2ff15041` |
| Advanced Masks | [v1.6.1](https://github.com/FiniteSingularity/obs-advanced-masks/tree/v1.6.1) | [advanced-source.zip](../third_party/background-engines/advanced-source.zip) | `4790c950f7f9aa441c1691da46755300dcf3839ed8c205a0df1ac34bcfe8c7e9` |
| RobustVideoMatting | [v1.0.0](https://github.com/PeterL1n/RobustVideoMatting/tree/v1.0.0) | [rvm-source.zip](../third_party/video-matting/rvm-source.zip) | `b58b1187fb9373f56db2e463a7629cf9f539c4c97ea361c793620396211122da` |
| BackgroundMattingV2 | [v1.0.0](https://github.com/PeterL1n/BackgroundMattingV2/tree/v1.0.0) | [bmv2-source.zip](../third_party/video-matting/bmv2-source.zip) | `f8bd03b63cebd6edbad860ce18b8422392dd330bad9216232b0808ea6566e906` |

These are original tagged archives. They include their original license files,
build manifests and source-level dependency references. They do not contain all
external SDKs, every dependency source tree, compiler tools or trained-model
checkpoint. The additional sources below are part of the rebuild references;
the four ZIP files alone are not described as a self-contained reproducible
build or complete corresponding-source certification.

## VEIL changes

- Face Beautifier: beauty geometry, slider mappings, regions, shader processing
  and One Euro smoothing ported into native C++/OBS effects. Native scanline
  coverage/blur replaces Canvas rasterisation. See [math contract](../tests/math-contract.md).
- Lumina: detector decoding, rotated ROI and landmark tracking adapted for
  BGRA/CPU inference. One caller-owned smoother replaces upstream smoothing.
  See [tracking notice](../third_party/face-tracking/NOTICE.md).
- Lite: upstream Windows DLL retained unchanged for old scenes. The effect-only
  [premultiplied-alpha patch](../third_party/background-engines/lite-premultiplied-alpha.patch)
  and [provenance](../third_party/background-engines/PROVENANCE.json) record the change.
- Advanced Masks: three unchanged C units and original shaders are compiled in
  a small separate VEIL wrapper. Exact original file hashes are recorded in
  [advanced-components/PROVENANCE.json](../third_party/background-engines/advanced-components/PROVENANCE.json).
  The full upstream DLL is also retained for independent existing filters.
- RVM/BMv2: official ONNX models are unchanged. VEIL supplies its own native
  runtime/tensor adapter, OBS lifecycle and temporal/output handling. Retired
  reference correction and photo storage are preserved separately on
  `archive/registered-background`; the normal product exposes automatic removal.
- ONNX Runtime: matting runtime DLL renamed to `veil-matting-ort.dll` for module
  isolation; its binary contents are unchanged. Original notices are retained.

## Building VEIL from these assets

See [BUILD.md](BUILD.md). The local bootstrap pins LLVM-MinGW and OBS source;
the SDK preparation script creates an import library from installed OBS. No
private preset or camera image is needed. Run the native build and fixture tests
before packaging. The installer source and its rollback self-test are included.

## Rebuilding upstream components

**Lite 4.0.1:** extract `lite-source.zip` and follow its `README.md`,
`CMakePresets.json`, `buildspec.json`, `.github` Windows build workflow and vcpkg
manifests. Production dependencies include CURL, fmt, ncnn, OBS/frontend API and
Qt6. Its registry pins are Microsoft vcpkg
`4ef2a7144d00c2f446029b9700e701a3094b8bfb` and the
[Kaito registry](https://github.com/kaito-tokyo/vcpkg-obs-kaito-tokyo/tree/1e678209cf07f4b413d3e05431126808bb4954f2).
Those port files identify source URLs, versions and hashes. The googletest/stb
submodules are not populated in the ZIP and are needed when upstream tests are
enabled; initialize them according to `.gitmodules`. The official Windows CI
preset disables that test option. Preserve every license shipped with the DLL;
the presence of a license file does not by itself prove that library was linked.

**Advanced Masks v1.6.1:** extract `advanced-source.zip`; follow its CMake,
bootstrap scripts, `buildspec.json` and Windows build workflow. It specifies OBS
`30.0.0-fixmac` and OBS dependencies/Qt `2023-11-03`, with archive hashes, and
fetches nlohmann/json `v3.11.3`. CURL and the matching OBS/Qt development inputs
are required for the complete upstream module. The narrow VEIL finishing helper
instead compiles the checked-in three C units with the VEIL build scripts.

**RVM models:** the tagged README links the original
[MobileNetV3 PyTorch checkpoint](https://github.com/PeterL1n/RobustVideoMatting/releases/download/v1.0.0/rvm_mobilenetv3.pth)
and [ONNX inference/export documentation](https://github.com/PeterL1n/RobustVideoMatting/blob/v1.0.0/documentation/inference.md).
The upstream exporter is on its separate `onnx` branch, not inside the tagged
ZIP. The branch was checked at
[`ebead27cb683e157b2bea7ca869daa820a07ba8f`](https://github.com/PeterL1n/RobustVideoMatting/tree/ebead27cb683e157b2bea7ca869daa820a07ba8f)
for this source index; this is not a claim that the original release was rebuilt
bit-for-bit from that later branch state. Model weights, exporter code and framework dependencies must be retained
when producing a newly converted model. The shipped ONNX hash, official release
URL and license are recorded in
[matting provenance](../third_party/video-matting/data/PROVENANCE.json).

**BMv2:** the tagged source and README describe the model/checkpoint/export
workflow. Its supplied HD ONNX model and MIT license are retained, including on
the archive branch; this is not the current person-removal model.

**Permissive runtimes and SDKs:** source/release URLs and exact hashes are in
[CPU runtime provenance](../third_party/onnxruntime/PROVENANCE.json),
[matting provenance](../third_party/video-matting/data/PROVENANCE.json),
[model provenance](../models/PROVENANCE.json) and the
[bootstrap script](../scripts/build/bootstrap_tools.py). Compiler-runtime license
and exception texts are copied into packaged plugin data by CMake.

**DirectML:** the proprietary Microsoft DLL is acquired from the official pinned
NuGet package. Its source code is not part of this repository, and no GPL/MIT
source grant is made for it. Retain the Microsoft license and third-party notices.
