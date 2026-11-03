# Background processing

The current product uses automatic person removal. Person, edge smoothing and
solid-colour keying own their controls in separate native OBS filters. The
normal build keeps `VEIL_REGISTERED_BACKGROUND=OFF` and never loads or deletes
previously captured reference photos.

Automatic removal uses the official RobustVideoMatting v1.0.0 recurrent model.
The adapter carries its four recurrent tensors between frames. DirectML inference
has a separate runtime module from CPU face tracking; initialization failure can
fall back to CPU, with the processing device shown in the status.

Camera → person removal or chroma key → optional outline smoothing → beauty.
Reconstructed RGB and alpha are paired with the source sequence and timestamp.
There is one replaceable pending input, so a busy worker receives the newest
available frame. Stale input or a terminal processing error uses original video.
This cannot reopen a stopped camera device.

The 0.4.5 conversion optimization uses equivalent SSE2/scalar calculations.
The model, input resolution, recurrence and quality calculations are retained in
0.4.6 and 0.4.7. Displayed processing time excludes capture, display and encoding;
it is not a measurement of total camera-to-broadcast latency.

Pinned upstream sources, model hashes, runtime licenses and notices are recorded
in `third_party/video-matting/data/PROVENANCE.json` and the component license files.
Legacy Lite and AdvancedMasks modules remain for scene compatibility and finishing
effects. Their exact source references and patches are retained in provenance.

## Retired captured-background work

`archive/registered-background` preserves the coherent 0.4.5 implementation for
research. It combines recurrent person recognition with conservatively bounded
reference-photo edge correction. The historical BMv2 adapter/model is retained
for comparison. This branch is not the current supported installation target.

The current source gate and retained test boundary are documented in
`features/registered-background/README.md`. Generated-plate and storage tests
protect archived code; they do not establish that this mode is enabled in the
current product or that a reference photo improves every natural scene.

## References

- RVM tensor/recurrence contract: https://github.com/PeterL1n/RobustVideoMatting/blob/master/documentation/inference.md
- BMv2 optional alignment: https://github.com/PeterL1n/BackgroundMattingV2/blob/master/inference_utils.py
- Colour-line matting reference: https://github.com/pymatting/pymatting/blob/master/pymatting/alpha/estimate_alpha_sm.py

VEIL's retained bounded colour projection is an independent small adaptation,
not an implementation of the complete Shared Matting algorithm. Its MIT notice
is supplied with the other component licenses.
