#pragma once
#include <array>
#include <cstdint>

namespace trueitem {
constexpr int landmark_count = 478;
struct Point3 { float x = 0, y = 0, z = 0; };
struct LandmarkFrame {
    std::array<Point3, landmark_count> points{};
    bool valid = false;
    float presence = 0;
    std::uint64_t timestamp_ns = 0;
};

// Names and units intentionally match the user's existing browser preset.
struct BeautyParams {
    int smooth = 100, texture = 26, even = 30, radius = 11, sharpen = 7;
    int slim = 1, chin = 0, eyes = 0, nose = 0, lips = 0;
    int eyeBright = 0, underEye = 17, lipAmount = 0, blushAmount = 0;
    int bright = 14, warmth = 71, vibrance = 100, contrast = 10;
    int glow = 45, vignette = 20, grain = 0;
    std::array<float,3> lip_rgb{196.f/255, 72.f/255, 95.f/255};
    std::array<float,3> blush_rgb{1, 138.f/255, 107.f/255};
    bool mirror = true;
    // VEIL additions are opt-in. Legacy 21-control presets keep their look.
    int skinBright = 0, redness = 0, teethWhite = 0, lipGloss = 0;
    int faceLength = 0, eyeSpacing = 0, noseLength = 0;
    int strength = 100, look = 0, lookAmount = 40;
    int preview = 0, split = 50;
    // Natural shape combines overlapping handles without cumulative stretching.
    // Stored browser control values remain unchanged; legacy rendering is optional.
    bool natural = true;
};
}
