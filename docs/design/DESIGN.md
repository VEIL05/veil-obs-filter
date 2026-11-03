# VEIL product design contract

Outcome: a broadcaster carries their exact saved camera look into OBS, then
adjusts face details without leaving OBS or operating a browser/server.

Use OBS's native, theme-aware properties, keyboard navigation, numeric slider
inputs, colour picker and file picker. Keep one visible editing section at a
time: Quick, Skin, Face details, Makeup, Light/colour, Preset files, Diagnostics,
Background removal.
Quick starts with overall strength, mirror, view and an explicitly applied preset.
The extra controls default to zero; no automatic retuning of the saved 21 values.

Face detail labels state the direction of signed controls. Zero keeps the
current face. All settings persist in the OBS filter. Old browser JSON imports
all 21 values, two colours and mirror exactly; VEIL JSON also saves additions.
Import validates everything before mutation. An invalid file keeps the look.
Restore is discoverable and restores the original personal preset.

Natural shape is enabled by default, keeps the stored values, blends overlapping
deformations as one bounded field and fades them with face presence. The checkbox
in Quick allows the old shape style. Background mode is Off/automatic RVM/
registered-background cleanup/Chroma, with one
explicit Apply button. Show only the selected mode's controls, name edge-stability
tradeoffs in plain Korean, and keep background processing before face colour.
Never silently remove/retune another filter. Main VEIL Background removal is the
single editing surface, including processing device. Managed person/finishing
filters show guidance/status, retaining hidden action callbacks for the main UI.
Identify ownership by marker + actual same-parent beauty UUID, never names;
orphan/independent helpers retain editing. OBS list eye controls remain native.
Migrate only owned stateless finishing roles to the native integration using
unchanged upstream C/shaders, retaining settings/name/order/enabled. Preserve the
person filter UUID and photo location. Adopt existing helper values once into
main controls without changing rendering, including fractional feather0..50 and
CPU choice. New v5 preset supports fractional feather; v1-v4 remain readable.
Explain that disabling beauty alone does not disable separate background effects.
Registered mode retains RVM human recognition and recurrence. The saved reference
may refine uncertain outer soft edges by at most8/255 and reduce faint matching
residue only after distributed photo-fit validation. Keep opaque body unchanged,
noise/cutoff transitions continuous and automatic temporal history pre-photo.
Never replace the human model with frame-independent BMv2. Keep the legacy BMv2 asset/adapter for old comparisons, not the live default.
Fixed-background registration is explicit: three-second leave-frame countdown,
one-second averaged capture, local storage notice, status/latency refresh and
reference deletion. Missing/size-mismatched reference uses RVM with a visible
reason. Retain the photo across resolution changes and reuse at its original
size. Complete pending reference saves/deletions before shutdown. Retake after
camera geometry or lighting changes. No reference in preset files.

Show tracker preparation, detected face, waiting for a face and initialization
failure in plain Korean. A refresh button updates the status without reopening
the camera. The native filter consumes OBS frames and never opens another camera.
Camera selection/resolution, recording, screenshots and final output use the
existing OBS controls; document their mapping instead of duplicating them.

Original/split/mask modes affect actual output. Label the mask warning directly
in the selector and explain returning to the processed view before broadcasting.
Do not claim a private preview. No hidden auto-save of images or network upload.

Installer: one primary install/update action, secondary reversible rollback,
and an always available close/cancel. Show readable Korean, installed location,
working/success/failure status, and actionable OBS-running errors. Preserve all
other plugins and OBS configuration. The installer never forcibly exits OBS.

Verification: real OBS property callbacks and GPU pixels are required for
behavior. Visible OBS UI and camera output remain a separate acceptance check.
An isolated test or mock screen cannot establish actual camera/UI success.
Latency improvements preserve model, resolution, recurrence ratio and all quality
math. Exact SIMD/scalar conversion and postprocess equivalence precedes timed
comparison. Separate model/CPU timing from OBS capture/upload and actual camera
latency; never improve a reported number by moving its timestamp boundary.

User copy: no model names (RVM/BMv2) or frame-queue counters in normal settings.
Use automatic background removal / remove using registered background / chroma.
A registered-background thumbnail is shown inside OBS properties. It comes from
the existing explicit reference, held in memory; no extra image file or changed
broadcast output. Empty/deleted references show an actionable placeholder.
