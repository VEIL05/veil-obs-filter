# Face tracking source and runtime provenance

The algorithm in `src/face_tracker.cpp` is adapted from the MIT-licensed
`src/engine/LmFaceDetector.cpp` of Lumina Beauty:

- Repository: https://github.com/dinjufen/lumina-beauty
- Fixed commit: `f1472d581dab9706c8536e0decd46592f9045d44` (2026-10-27).
- Original source: https://github.com/dinjufen/lumina-beauty/blob/f1472d581dab9706c8536e0decd46592f9045d44/src/engine/LmFaceDetector.cpp
- Copyright (c) 2026 Lumina Beauty contributors.
- Full license: `LICENSE-LUMINA.txt` in this directory.
- The original source is available at the fixed-commit link above. A full Lumina
  checkout is not bundled in this repository or required by the installed plugin.

Only the detector, landmark ROI and tracking algorithm were reused. Lumina's
OpenGL renderer, OBS plugin, effect engine and segmentation models are not used
or linked. This adapter uses direct BGRA bilinear sampling instead of an I420
conversion, keeps the original RGB normalization (detector [-1,1], landmarks
[0,1]), 896 anchors, 128/256 model sizes, thresholds 0.5, 1.5 ROI expansion,
rotated ROI refinement and lost-face reacquisition. Upstream's adaptive temporal
smoothing is deliberately omitted: the caller owns the single OneEuro stage so
it can match the existing browser preset without double-filtering or extra lag.
It adds normalized z coordinates, input/output validation, RAII model sessions,
timestamp preservation and explicit CPU runtime loading.

The two model assets are converted Google MediaPipe weights redistributed by
that pinned upstream commit under Apache-2.0. See `models/NOTICE.md` and the full
`models/LICENSE-Apache-2.0.txt`; their upstream model-card links are retained.

ONNX Runtime is the official Microsoft CPU Windows x64 release 1.31.1:

- https://github.com/microsoft/onnxruntime/releases/tag/v1.31.1
- Archive: `onnxruntime-win-x64-1.31.1.zip`.
- SHA-256: `5869e26bb25314375f2d5ef1877e3ea06b1543d6c710e6e4a1ec2f7177ab4b2c`.
- The downloaded archive matched the official GitHub release-asset API digest.
- Its full MIT license and ThirdPartyNotices are retained under
  `third_party/onnxruntime/` and must accompany distributed runtime binaries.
- The C++ header API uses `ORT_API_MANUAL_INIT`; `OrtGetApiBase` is obtained from
  the explicitly loaded absolute runtime DLL path. There is no import-library
  dependency on onnxruntime.dll. CPU execution only, intra-op threads 1–4,
  sequential execution, idle thread spinning disabled, telemetry disabled.

`FaceTracker` does no network I/O, camera access, image recording or UI access.
Its caller owns downsampling, asynchronous scheduling, frame age policy and
rendering. The caller must provide top-down BGRA bytes (not RGBA) and call each
instance serially. Coordinates are unmirrored input-space x/width, y/height,
z/width. Out-of-frame coordinates are intentionally not clamped. The caller's
`timestamp_ns` field is preserved on both success and failure.

## Public test fixture

`fixtures/astronaut.png` is the Eileen Collins NASA photograph distributed by
scikit-image 0.21.0. The project's official documentation identifies it as public
domain, with no known copyright restrictions:

- https://scikit-image.org/docs/0.21.x/api/skimage.data.html#skimage.data.astronaut
- Original downloaded fixture: https://raw.githubusercontent.com/scikit-image/scikit-image/v0.21.0/skimage/data/astronaut.png
- SHA-256: `88431cd9653ccd539741b555fb0a46b61558b301d4110412b5bc28b5e3ea6cb5`.
- `astronaut-320.bgra` is its 320x320 bilinear resized BGRA derivative.

This fixture tests inference locally only. No user's face/camera frame was
used or uploaded. It is not part of the plugin's runtime requirements.

## Standalone verification

`tracker_test.cpp` tests the two actual ONNX sessions with the public fixture,
478 finite normalized points, orientation, caller timestamp, padded BGRA
stride, black/no-face input, invalid stride rejection, reacquisition and shared
runtime lifetime across two instances. `tracker_test.exe` takes arguments:

```
tracker_test.exe MODEL_DIRECTORY ABSOLUTE_ONNXRUNTIME_DLL_PATH astronaut-320.bgra
```

The initial local run on 2026-11-03 passed. Its 30 tracked static frames measured
8.21 ms median / 9.11 ms p95 with two intra-op threads. This measures inference
only; it is not an OBS rendering/encoding/frame-rate benchmark or a guarantee
for other CPUs, poses or lighting.
