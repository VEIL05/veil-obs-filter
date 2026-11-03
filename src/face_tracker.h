#pragma once
#include "beauty_types.h"
#include <filesystem>
#include <memory>
#include <string>

namespace trueitem {
// Serial calls only. Input bytes are B,G,R,A, top row first; no mirroring.
// x/y are normalized to the input width/height, z to input width. Coordinates
// may lie outside [0,1] for a partly off-screen face. timestamp_ns belongs to
// the caller and is preserved. Temporal smoothing is caller-owned so the OBS
// worker can apply its single OneEuro stage. No camera or UI access occurs.
class FaceTracker {
public:
    FaceTracker();
    ~FaceTracker();
    FaceTracker(const FaceTracker&) = delete;
    FaceTracker& operator=(const FaceTracker&) = delete;
    bool initialize(const std::filesystem::path& model_dir,
                    const std::filesystem::path& runtime_dll_path,
                    int threads, std::string& error);
    bool detect_bgra(const std::uint8_t* pixels, int width, int height,
                     int stride, LandmarkFrame& frame);
    void reset_tracking();
    const std::string& last_error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string error_;
};
}
