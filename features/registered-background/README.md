# Archived reference-background feature

User decision2026-11-03: stop supporting captured-background removal in the
current product; keep automatic person removal. Preserve this work for a separate
future Git commit, rather than discarding it or mixing it into the main feature.

Default: `VEIL_REGISTERED_BACKGROUND=OFF`. The product has no registration mode,
photo buttons or preview, does not load/write/delete reference files, and maps
old mode3 to automatic1. Existing local photos remain untouched. The source gate
is compile-time, with no runtime enable switch in user settings.

Retained feature boundary:
- src/registered_background_feature.h and CMake option.
- src/video_matting_filter.cpp guarded registration/storage/preview/worker path.
- src/background_refinement.cpp/.h registered-photo edge refinement.
- src/reference_preview.cpp/.h thumbnail encoder.
- src/matting_model.cpp/.h legacy calibrated model adapter.
- tests/reference_storage_test.cpp and background_refinement_test.cpp.
- tests/background_smoke.cpp historical generated-plate integration section.

The source bundle includes this boundary and its licenses. No private registered
photo or camera frame is included. The storage/refinement tests remain regression
checks for retained source, not evidence that the default product offers this mode.

Git preservation: `archive/registered-background` holds the coherent historical
0.4.5 implementation, separately from the automatic-only current product on `main`.
It is a research archive, not the current installation target.
Enabling the old CMake option is developer-only and requires renewed integration
verification; it is not a current supported product variant.
