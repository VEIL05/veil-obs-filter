# Video input clock and diagnostics (0.4.3)

Purpose: distinguish stale input/render capture from a stalled model worker when
enabling a VEIL filter freezes the displayed camera frame. No automatic restart,
device reconfiguration, model replacement or new camera connection is added.

0.4.3 uses the always-active source-frame clock for processing as well as logging.
An asynchronous camera frame is captured at most once for each engine, even when
multiple previews render its cached texture. A gap over 500ms uses OBS's direct
passthrough without another GPU readback/model submission. In-flight results from
the previous generation are discarded. Resuming frames or a backwards source
timestamp resets temporal history automatically. Synchronous sources retain
time-based processing because they do not provide asynchronous frame callbacks.
The matting worker requests fresh capture when ready, avoiding an older pending
image. Terminal worker errors also bypass graphics/readback until Apply retries.
These changes cannot reopen a stopped capture device and are not presented as a
verified correction of the actual-camera input failure.

`[VEIL trace]` lines go into the existing OBS log at most once every five seconds
per filter. An idle worker also emits this heartbeat, so a blocked graphics thread
does not prevent all diagnostics. Disabled filters emit one disabled heartbeat
and then become quiet. Only numeric metadata and an abbreviated filter UUID are
logged. No pixels, sampled hashes, images, reference thumbnails or camera names
are logged. Existing OBS log retention applies.

- `source`, first `changed`, `source_ts`, `source_age_ms`: asynchronous frames OBS
  passes through the filter callback, sample-change count, last frame timestamp
  and age. The callback returns the identical pointer without retaining or
  modifying it. These are frames consumed by OBS, not a driver-receive counter.
  If the graphics thread stops consuming frames, an increasing age alone does
  not establish that the physical camera stopped delivering them. Synchronous
  input sources do not invoke this callback.
- `render`, `tick`: render and video-tick entry counts. `phase`: 0 outside render,
  1 entering/validating, 2 OBS filter begin, 3 copying the source into the raw
  render target, 4 GPU readback/submission, 5 output/beauty shader work.
- `render_ms`, `max_render_ms`, `render_age_ms`: last completed render duration,
  lifetime maximum, and time since the latest render entry.
- `capture`, second `changed`, `alpha`: mapped GPU captures, sampled change count
  and mean alpha at 64 points. The beauty capture is a downscaled tracker input;
  the matting capture is full size. Their alpha values need not match when other
  filters are between them.
- `begin_fail`, `map_fail`, `target_fail`: failed OBS filter begins, GPU maps and
  raw render-target begins. Errors that previously returned early are counted.
- `work=started/finished`, `seq=started/finished`, `work_ms`, `work_age_ms`: model
  attempts, their input identifiers, completed duration and age of latest start.
  Beauty uses its staged frame timestamp as the identifier. A finished attempt
  can be unsuccessful or discarded after a generation change; check `errors`
  and the existing descriptive exception logs as well.
- `errors`: tracker errors or matting exception/fallback events. Recovery may
  generate more than one event for the same frame. Normal 'no face found' is not
  counted as a tracker error.

Packed and planar formats use their first plane's valid byte width, so sampling
covers the image width without counting row padding as motion.
No call to `obs_source_get_frame` is used for observation: that API consumes the
current async frame. The small image sample is hashed only in memory and skips
row padding. It indicates changes; it is not a proof of perceptual motion.
User-visible RGB progression is independently checked in the integration test.

Verification uses a public NASA fixture and generated backgrounds in isolated
libobs. It switches brightness, toggles both beauty and matting eyes four times,
and checks eight actual output-RGB changes while retaining the face. Additional
async, two-view, reference capture/preview/reload/delete and chroma paths run in
the same harness. This is not evidence that the real-camera freeze is fixed.
