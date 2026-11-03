# VEIL 0.4.4 background processing — 2026-11-03

## Current registered-mode decision

Retain RVM human recognition and recurrence in both ordinary-background modes.
Old registered mode replaced it with independent BMv2 inference. Registered
photos now supply conservative soft-edge evidence, not a second person detector.
BMv2 adapter/assets remain only for historical comparison.

Primary references:
- RVM recurrence: https://github.com/PeterL1n/RobustVideoMatting/blob/master/documentation/inference.md
- BMv2 optional photo alignment: https://github.com/PeterL1n/BackgroundMattingV2/blob/master/inference_utils.py
- Shared Matting colour-line component: https://github.com/pymatting/pymatting/blob/master/pymatting/alpha/estimate_alpha_sm.py
  MIT copyright2020 PyMatting; retained in data/licenses/PyMatting-MIT.txt.
  VEIL independently implements the small projection, not all Shared Matting.

Photo fit requires at least64 confident-background samples, inliers across6/16
screen tiles, at least85% residual inliers and channel exposure offsets<=24.
A mismatch keeps the automatic output byte-identical. This is a fit/rejection
check, not homography alignment. Camera/lighting changes may require recapture.

At uncertain outer pixels, estimate alpha from observed colour I, RVM foreground
F and corrected photo B using dot(I-B,F-B)/(||F-B||^2+epsilon). F is recovered
from the premultiplied RVM result; it is not substituted with I. Require matching
neighbouring background, sufficient F/B colour separation, small reconstruction
residual and agreement with the RVM estimate. Confidence tapers at eligibility
boundaries; measured/background quantization noise defines a correction deadband.
Corrections are capped8/255; alpha>=240 remains untouched. Similar foreground/
background colour and bad evidence retain the automatic result.

Faint residue<=12 gets smoothly tapered reduction only away from predicted
person details. The old12->0 /13->13 cutoff is removed. Save AUTOMATIC temporal
history before all photo corrections, so a photo decision cannot accumulate into
next-frame hair/body erosion. No additional temporal averaging/optical flow.

Independent synthetic alpha tests exercise the actual worker postprocess helper:
known soft-edge error20 ->18 alpha levels for two deliberately biased estimates;
longitudinal RGBnoise+/-3 leaves a correct128-alpha edge unchanged;12/13 cutoff,
photo-history feedback, wrong textured photo, same-colour detail and moving thin
detail canaries PASS. This limited analytic fixture is not natural-camera proof.
Existing portrait sensitivity metrics in work/quality-044-recurrent used RVM-derived
reference alpha and bypassed worker postprocess; they are historical exploratory
evidence, not an unbiased model ranking or a claim of visible contour improvement.

Pending reference writes/deletions drain before worker shutdown. Tests reproduce
old lost-write behavior, exact replacement, separate-process restore/thumbnail,
and deletion. Photos stay outside updater target/rollback and survive size changes.
Actual camera appearance and installed restart acceptance remain separate.

--- Historical 0.4 adoption and measurements (superseded where stated) ---

Outcome: retain face/person and soft hair edges without a green screen, including
an empty-background mode for a fixed camera; preserve saved beauty/audio values.

## Adoption decision and evidence

Lite4.0.1 in VEIL0.3 passed public still tests, but the user reported face holes
and flicker. It is no longer the ordinary-background default. Keep its module
solely to load old scenes; Apply disables the owned legacy role.

Video-editing research: MatAnyone2 is a quality reference, but its S-Lab NC terms
need separate product permission. VideoMaMa code has NC terms. VideoMaMa++ and
MatAnyone-E had not released inference code/weights. None is copied or bundled.

BGOBS0.3.26 Windows pilot used CPU, reported186–297ms at720p and failed to save
the plate because its OpenCV build lacked a PNG writer. Evidence in
work/matting-pilot/run1*. Do not install that wrapper/phone/USB/ADB/update checker.

Use unmodified official PeterL1n/RobustVideoMatting v1.0.0 (GPL3) and
PeterL1n/BackgroundMattingV2 v1.0.0 HD (MIT) through their published tensor
contracts. RVM carries four actual recurrent tensors forward. BMv2 consumes
current RGB plus a clean plate and outputs soft alpha and reconstructed RGB.
Official-model720p benchmark on RTX2070SUPER/Ryzen3900X: RVM median25.0ms,
p9527.5ms; BMv2 median31.9ms,p9533.3ms. Includes CPU conversion and inference,
excludes OBS capture/display/encoding. Actual camera quality is separate.

## Detachable boundary and refresh

src/matting_model.cpp is the tensor adapter; video_matting_filter.cpp owns OBS
capture/queue/reference/output. Official runtime/models, local SHA256 provenance,
notices and both corresponding upstream source archives accompany the source ZIP.
Legacy model releases publish no digests; BMv2 also matches the model in the
SHA-verified BGOBS package. URLs/digests: third_party/video-matting/data/PROVENANCE.json.

Matting DirectML ORT1.24.4 has its own headers/API table and renamed DLL inside a
separate module; face CPU ORT1.31 remains separate. No Python, extra camera,
browser, global runtime replacement or upload. Telemetry is disabled. GPU setup
failure tries CPU and labels it; CPU can be too slow for live use.

Camera → selected video matte/chroma → optional feather → beauty. Reconstructed
RGB and alpha are paired to the exact source sequence/timestamp. Queue has one
waiting frame, replaced by newer input. Static-pixel cleanup is mild and motion
gated. Model error or stale result displays original video with an error status.

Update pinned runtime/model directory, review tensor/backend contracts, retain
upstream notices/source, then run GPU harnesses. Remove through background Off
and installer rollback. Older VEIL cannot process the new video filter; remove
generated video filters before rolling back. No shared installed OBS module is
replaced. Lite remains only for old-scene compatibility.

## Reference lifecycle

Explicit Capture →3sec countdown→1sec averaged empty frame. Atomic local storage
per source UUID: moduleconfig/veil-video-matting/backgrounds/*.veilbg. It is not in
presets, scene settings or source/delivery archives. Delete removes the file and
uses RVM. Missing/mismatched resolution falls back visibly. Retake after camera,
crop, rotation, resolution or lighting changes. OBS deferred source updates must
retain a pending calibrated capture; unchanged updates do not reset recurrence.

## Verification boundary

tests/background_smoke.cpp exercises actual DLLs, VEIL Apply/capture/refresh/
delete, moving/noisy/dimmed public face core, premultiplied edges, plate save/load,
filter recreation, fallback, chroma, exact Off and unrelated-filter canary.
tests/gpu_smoke.cpp covers all beauty controls and preset versions1–4.
Public/generated tests do not prove all hair/hands or the user's face survive.
Actual720p movement/occlusion, encoding and longer stability remain separate.

## 0.4.1 user-facing copy and reference preview

Normal property labels use plain Korean without model names. Explicitly registered
references have an in-memory WIC PNG thumbnail in OBS QLabel rich text (data URI).
Viewing/reopening/deleting a reference updates that thumbnail; no extra image file
is persisted. Actual0.4 Auto worked then disappeared once and recovered after
user restart; no error was logged. Root cause remains unresolved.0.4.1 rejects
non-finite outputs through existing GPU/CPU recovery and logs only aggregate
empty-output metadata.1080p async/multiview1800-frame public leave/return test
passed but did not reproduce the actual failure.
