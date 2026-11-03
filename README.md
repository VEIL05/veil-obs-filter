# VEIL 0.4.5 · Captured-background archive

This branch preserves the historical VEIL captured-background implementation
requested for separate Git history. It is a development reference, **not the
current supported VEIL installer**. Use the main branch for the current product.

The snapshot comes from the delivered VEIL 0.4.5 source bundle. Historical face,
matting, reference storage/preview, refinement and shader code are preserved.
No personal preset, captured background, camera image, OBS profile, build output
or installer is included. The supplied astronaut image is a public NASA test
fixture, documented in the tracking notice.

## What is preserved

- Automatic video matting and the historical captured-background mode.
- Empty-background registration, local persistence, preview and deletion.
- Conservative reference-based refinement on top of recurrent person detection.
- Face, skin, makeup and colour processing from the 0.4.5 snapshot.
- The matching runtime/model dependencies, notices and regression fixtures.

The captured-background approach did not provide enough practical improvement
to remain in the active product. This branch does not claim higher quality than
automatic removal. Historical test results are not evidence of current live-camera
quality or stability on other computers.

## Rebuild the snapshot

See [BUILD.md](docs/BUILD.md). The target is Windows x64 with OBS Studio 33.0.1.
This branch retains the historical 0.4.5 build/version and installer source.
Rebuilding an installer does not make it a current update.

Small checkout accommodations are included: project-relative bootstrap scripts,
a neutral preset fallback in CMake and GPU smoke, explicit smoke preset selection,
and generic build instructions. Existing local personal preset behaviour is
preserved without committing personal values.

Source URLs, version identifiers, recorded SHA-256 values and dependency
provenance remain intact. Historical product source is unchanged apart from the
preset packaging and smoke-test accommodations described above.

See [사용법.txt](사용법.txt) for the historical registration flow. Evaluate this
branch separately from current OBS scenes and keep backups before installing a
historical build.

## Licenses

Retain [LICENSE](LICENSE), [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) and all
component notices. The video adapter and RVM include GPL-3.0 code, the beauty
code is GPL-2.0-or-later, and other components retain their own terms. DirectML
has separate Microsoft redistribution terms. The provenance records link the
pinned upstream assets; duplicate upstream ZIP archives are not tracked here.

## Acknowledgements and source licenses

[Face Beautifier](https://github.com/ctbot000/face-beautifier),
[Lumina Beauty](https://github.com/dinjufen/lumina-beauty),
[RobustVideoMatting](https://github.com/PeterL1n/RobustVideoMatting),
[BackgroundMattingV2](https://github.com/PeterL1n/BackgroundMattingV2),
[Advanced Masks](https://github.com/FiniteSingularity/obs-advanced-masks) and
[Lite](https://github.com/kaito-tokyo/live-backgroundremoval-lite) are acknowledged
with their exact versions and reuse scope in [CREDITS.md](CREDITS.md).
See [LICENSE](LICENSE), [module/asset scope](docs/LICENSING.md), and
[pinned source archives and patches](docs/DEPENDENCY-SOURCES.md).
Microsoft DirectML retains its separate Microsoft terms and is not open source.
