# VEIL OBS Filter 0.4.5

This OBS plugin is licensed under GPL-2.0-or-later. Source accompanies the local
distribution. It links to libobs; OBS Studio copyright belongs to its contributors.
The plugin does not include or redistribute OBS Studio itself.

## Face beauty mathematics and shaders

ctbot000/face-beautifier, commit
`ca5ad40b58e2d0133de6b22282b29000d3667046`, Copyright (c) 2026 CTBot000,
MIT. https://github.com/ctbot000/face-beautifier

The original slider mappings, geometric warp, facial regions, skin/light/colour
shader and One Euro coordinate smoothing are ported to C++/OBS effects. Canvas
mask rasterisation is replaced by native scanline coverage and Gaussian blur.
VEIL adds opt-in face length, eye spacing and nose length controls, skin light
and redness adjustment, teeth whitening, lip gloss, colour looks, preview modes,
and an overall strength blend. The warp capacity is extended to 24 handles.
See Face-Beautifier-LICENSE.txt in the installed data/licenses directory.

## Face tracking

dinjufen/lumina-beauty, commit `f1472d581dab9706c8536e0decd46592f9045d44`,
MIT. https://github.com/dinjufen/lumina-beauty

Only the CPU detector/478-point landmark decoding and rotated ROI tracking are
adapted. Internal adaptive smoothing was removed; one One Euro smoother runs in
our worker. Lumina's OpenGL rendering/beauty/segmentation are not included.
See LICENSE-LUMINA.txt and Face-Tracking-NOTICE.md.

The two ONNX model files retain Apache-2.0 notices and pinned provenance in
data/models/. They originate from Google's MediaPipe face models via Lumina.

## ONNX Runtime

Microsoft ONNX Runtime CPU Windows x64 1.31.1, MIT.
https://github.com/microsoft/onnxruntime/releases/tag/v1.31.1
The official binaries and their ThirdPartyNotices are included unchanged.
The face tracker uses local CPU inference. Telemetry and spinning are disabled.

## Video matting (VEIL 0.4)

Official PeterL1n/RobustVideoMatting v1.0.0 MobileNetV3 FP32 ONNX, GPL-3.0,
https://github.com/PeterL1n/RobustVideoMatting. Official
PeterL1n/BackgroundMattingV2 v1.0.0 MobileNetV2 HD ONNX, MIT,
https://github.com/PeterL1n/BackgroundMattingV2. Models are unchanged. Both tagged
source archives accompany the source ZIP, with model licenses and SHA256 provenance.
The adapter follows their published tensor contracts and preserves RVM recurrent
states. No BGOBS wrapper, phone/USB component, or external model service is used.

Microsoft.ML.OnnxRuntime.DirectML 1.24.4 NuGet, MIT, and Microsoft.AI.DirectML
1.15.4 redistributable, Microsoft DirectML terms. Only the Windows x64 release
DLL is included. The unmodified ORT DLL is renamed veil-matting-ort.dll to isolate
it from OBS's and the face tracker's runtime; it runs in a separate OBS module.
Their licenses and third-party notices are under data/engines/video/data/licenses.
ORT telemetry is disabled. Inference is local; GPU failure falls back to CPU.
An empty background is stored only after the explicit registration button, in
the module config backgrounds directory, and can be deleted in the same UI.
No captured reference or user frames are included in source/delivery archives.

## Compiler runtime

LLVM-MinGW UCRT 20261020 is a development tool, not required after installation.
Its libc++/unwind/MinGW/winpthreads runtimes are statically linked. Their license
and exception notices are included in data/licenses/.

## Data and network

The VEIL processor opens no independent camera and processes frames OBS supplies.
The bundled Lite engine's automatic version-check and first-run flags are
disabled for this bundled instance; updates are provided through VEIL. Existing
independently installed upstream modules/configuration are reused as-is.
The preset stores settings only, including original controls, additions, natural
shape mode and background choices. It stores no frames or device IDs.
The NASA astronaut public fixture is used only by development tests and is not
installed. User camera images and OBS account/stream data are not packaged.

## Background engines

Live Background Removal Lite 4.0.1, Kaito Udagawa and contributors,
GPL-3.0-or-later. https://github.com/kaito-tokyo/live-backgroundremoval-lite/releases/tag/4.0.1
Official Windows binary is unchanged. The bundled `main.effect` has a documented
premultiplied-alpha compatibility patch for chaining with OBS filters. Inference,
guided refinement and temporal averaging are upstream implementations.
Original corresponding source: `third_party/background-engines/lite-source.zip`.
Its component notices (ncnn, wolfSSL, curl, fmt and others) are retained under
`data/engines/lite/data/licenses`; the compiled MediaPipe model is upstream's.

Advanced Masks 1.6.1, FiniteSingularity and contributors, GPL-2.0.
https://github.com/FiniteSingularity/obs-advanced-masks/releases/tag/v1.6.1
Official Windows binary/data are unchanged and retained for independently used
Advanced Masks filters. VEIL0.4.5's small native finishing module compiles three
unmodified GPL C components (mask-chroma-key.c, mask-feather.c, obs-utils.c) and
uses the same bundled shaders. Its wrapper retains the upstream get_input_source
operations, settings/update/render mathematics and alpha/color-space semantics,
while giving managed helpers an informational properties page. Qt/SVG/Font
Awesome/network code is not included in this helper. The complete original
source archive accompanies the source distribution in
`third_party/background-engines/advanced-source.zip`; selected unchanged file
hashes are in `advanced-components/PROVENANCE.json`.

Pinned release SHA-256 and per-file provenance are in `data/engines/PROVENANCE.json`.

## Photo-guided soft-edge refinement reference

The colour-line alpha projection is independently implemented in native C++
following the mathematical component used by PyMatting Shared Matting. This does
not embed the complete Shared Matting algorithm or its Python/Numba runtime.
PyMatting copyright(c)2020 PyMatting, MIT license.
Source: https://github.com/pymatting/pymatting/blob/master/pymatting/alpha/estimate_alpha_sm.py
License: data/licenses/PyMatting-MIT.txt (also in source third_party/background-engines).
Local validation adds photo-fit, colour/noise uncertainty, soft eligibility and
8/255 correction guards; actual-camera quality is verified separately.
