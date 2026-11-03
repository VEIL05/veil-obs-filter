# Native beauty math and effect contract

Upstream: `ctbot000/face-beautifier`, commit
`ca5ad40b58e2d0133de6b22282b29000d3667046`, MIT, copyright CTBot000 2026.
Original math modules: `controls.js`, `face.js`, `warp.js`, `landmarks.js`.
Original image pipeline: `mask.js`, `renderer.js`, `shaders.js`.

## Core API

`compute_uniforms(frame, params, source_width, source_height)` does not allocate
textures, touch OBS or perform inference. Input points are 478 normalized x/y/z
landmarks in the original, unmirrored source orientation, top-left origin. Valid
frames with positive presence yield geometry. Invalid/nonfinite/degenerate frames
yield zero face confidence and no warps; global colour remains enabled, matching
the upstream skin-tone fallback. z is intentionally unused by the 2D beauty math.

`CoreUniforms.shader` mirrors the upstream `toShader` names and units. Set scalar
effect uniforms by their corresponding `uSmooth`, `uTexture`, `uEven`, etc. names.
`lipColor`/`blushColor` are gamma-encoded RGB floats, not linearised. `radius` and
geometry fields slim/chin/eyes/nose/lips are CPU-only inputs, not effect uniforms.

Bind `aspect` to `uAspect`, `face` to `uFace`, `mirror` as float to `uMirror`,
`warp_count` to `uWarpCount`, entire contiguous `warp_p`/`warp_q` float4 arrays to
`uWarpP`/`uWarpQ`; eye_l/r and cheek_l/r to matching `uEyeL` etc. Set `uTime` in
seconds and `uCompare=-1` for the production output. Maximum warp count is20.

`build_mask(frame, aspect, 320)` returns unmirrored, top-left RGBA8 pixels, row
stride `width*4`. At16:9 it is320×180. Alpha is always255; RGB channels have exactly
the original semantic roles: red skin excluding features, green lips, blue eye
openings and inner mouth. Missing face produces black RGB for fallback. Cache the
mask per tracking result; do not regenerate it synchronously on every render.

Mask geometry/index lists/scales, Gaussian standard deviations, channel addition
and multiplicative feature exclusion are ported from upstream. Browser Canvas2D
antialiasing/blur implementations are not byte specified. Native rasterisation
uses four vertical subscanlines with analytic horizontal pixel coverage, then a
normalized separable Gaussian truncated at3sigma. It is a faithful region/feather
port but **not claimed byte-identical to Skia Canvas2D**. Pixel-equivalence and live
visual tuning are separate integration checks; numeric geometry is independently
tested against upstream JS.

## Five-pass effect

All source and render targets must retain upstream gamma-encoded sRGB texel values
in ordinary UNORM textures; do not automatically decode/encode or blend in linear
space. Source and mask use the same unmirrored orientation.

1. src→halfA with `uTex=source`, `uStep=(blur_scale*1.15/halfW,0)`.
2. halfA→halfB with `uStep=(0,blur_scale*1.15/halfH)`.
3. halfB→quarterA with `uStep=(blur_scale*2.6/quarterW,0)`.
4. quarterA→quarterB with `uStep=(0,blur_scale*2.6/quarterH)`.
5. `Composite`: `uSrc=source`, `uFine=halfB`, `uWide=quarterB`, `uMask=CPU mask`.

Half and quarter targets follow the source aspect. `Blur`, `BlurHorizontal` and
`BlurVertical` are aliases of the same shader; `uStep` determines its direction.
Only final `Composite`/debug `Mask` mirrors the source coordinates. OBS `ViewProj`
and standard POSITION/TEXCOORD0 sprite vertices drive `VSDefault`; no WebGL flipY
is needed because OBS intermediate/source textures share top-left convention.

The composite preserves every original effect equation, including the gamma-space
blur, local-warp rim-flat falloff, YCbCr skin test, texture retention, tone/sharpen,
under-eye/whitening, lip/blush, glow/colour/vignette/grain, and optional comparison.
One undefined GLSL case is written explicitly: upstream reversed-edge
`smoothstep(0,-.055,x)` becomes `smoothstep(0,1,-x/.055)`, preserving its observed
algebra while avoiding reversed-edge ambiguity in HLSL.

## Validation and limits

`math_reference.mjs` imports the unchanged upstream JS and its synthetic478-point
fixture. It generates references for Off, Glam, and the actual saved21-value user
preset at widths1280 and1920 (height720). Native math matches warps, eye/cheek
uniforms and blur scale within2e-6. Tests also enforce neutral UV identity, move
cap0.45r, actual user colour/radius mapping, mask feature exclusion, and missing
face fallback. Build `math_test.cpp` with C++17 and assertions enabled (`-UNDEBUG`
for Release). The reference file is checked in; Node is not needed to run tests.
To regenerate, run `node tests/math_reference.mjs <upstream-face-beautifier/src>`
against the exact pinned upstream commit above, retaining its MIT attribution.

`math_shader_compile.py` compiles PSBlur/PSComposite/PSMask with Windows D3DCompiler
in CPU-only mode after minimal effect-container syntax adaptation. It checks HLSL
syntax but does not replace OBS effect parsing or an actual GPU pixel test.

Initial optimized local mask call was about30ms; it is a worker/cache operation,
not a render FPS measurement. End-to-end OBS source capture, tracking coordinates,
gamma behaviour, effect bindings, GPU pixel equivalence and live performance must
be validated by the integrating OBS runtime before claiming a working native filter.
