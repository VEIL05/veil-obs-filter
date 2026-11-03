// Adapted from ctbot000/face-beautifier, commit ca5ad40b58e2d0133de6b22282b29000d3667046.
// Copyright (c) 2026 CTBot000. MIT; see third_party/face-beautifier/LICENSE.
#pragma once
#include "beauty_types.h"
#include <array>
#include <cstdint>
#include <vector>

namespace trueitem {
constexpr int max_warp = 24;
using Vec2 = std::array<float, 2>;
using Vec3 = std::array<float, 3>;
using Vec4 = std::array<float, 4>;

struct ShaderParams {
    float smooth{}, texture{}, even{}, sharpen{}, radius{};
    float slim{}, chin{}, eyes{}, nose{}, lips{};
    float eyeBright{}, underEye{}, lipAmount{}, blushAmount{};
    Vec3 lipColor{}, blushColor{};
    float bright{}, warmth{}, vibrance{}, contrast{}, glow{}, vignette{}, grain{};
    float skinBright{}, redness{}, teethWhite{}, lipGloss{};
    float faceLength{}, eyeSpacing{}, noseLength{};
};

struct CoreUniforms {
    ShaderParams shader{};
    float aspect = 1, face = 0, blur_scale = 1.1f;
    int warp_count = 0;
    std::array<Vec4, max_warp> warp_p{}, warp_q{};
    Vec4 eye_l{0, 0, .001f, 0}, eye_r{0, 0, .001f, 0};
    Vec4 cheek_l{0, 0, .001f, 0}, cheek_r{0, 0, .001f, 0};
    Vec4 mouth{0, 0, .001f, 0};
    bool mirror = false;
    bool natural = true;
};

struct MaskImage {
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
};

ShaderParams to_shader(const BeautyParams &params);
CoreUniforms compute_uniforms(const LandmarkFrame &landmarks, const BeautyParams &params,
                              std::uint32_t width, std::uint32_t height);
// Unmirrored, top-left origin, RGBA8 mask. Update only when landmarks change.
// Invalid landmarks produce black RGB with opaque alpha (skin-tone shader fallback).
MaskImage build_mask(const LandmarkFrame &landmarks, float aspect, int width = 320);
// Same backward map as the composite shader, exposed for equivalence tests.
Vec2 apply_warp(Vec2 uv, const CoreUniforms &uniforms);
} // namespace trueitem
