#include "face_tracker.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace trueitem;
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        require(argc == 4, "Usage: tracker_test model_dir runtime_dll astronaut-320.bgra");
        std::ifstream file(argv[3], std::ios::binary);
        std::vector<uint8_t> pixels((std::istreambuf_iterator<char>(file)), {});
        require(pixels.size() == 320 * 320 * 4, "Public fixture size differs");
        FaceTracker tracker;
        LandmarkFrame frame; frame.timestamp_ns = 12345;
        require(!tracker.detect_bgra(pixels.data(), 320, 320, 1280, frame), "Uninitialized detector accepted input");
        std::string error;
        require(tracker.initialize(argv[1], argv[2], 2, error), error.c_str());
        require(tracker.detect_bgra(pixels.data(), 320, 320, 1280, frame), tracker.last_error().c_str());
        require(frame.valid && frame.presence >= .5f && frame.timestamp_ns == 12345, "Face frame contract failed");
        for (const auto& p : frame.points)
            require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z), "Nonfinite landmark");
        require(frame.points[33].x < frame.points[263].x, "BGRA/image coordinate eye order is incorrect");
        require(frame.points[10].y < frame.points[152].y, "Forehead/chin ordering is incorrect");
        const float presence = frame.presence;
        const auto nose = frame.points[1];
        std::vector<double> milliseconds;
        for (int i = 0; i < 30; ++i) {
            const auto start = std::chrono::steady_clock::now();
            require(tracker.detect_bgra(pixels.data(), 320, 320, 1280, frame), "Tracking lost static public fixture");
            milliseconds.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        }
        std::sort(milliseconds.begin(), milliseconds.end());
        // Padded rows must give the same face coordinates after reset.
        std::vector<uint8_t> padded(320 * 1296, 0);
        for (int y = 0; y < 320; ++y)
            std::copy_n(pixels.data() + y * 1280, 1280, padded.data() + y * 1296);
        tracker.reset_tracking();
        require(tracker.detect_bgra(padded.data(), 320, 320, 1296, frame), "Padded BGRA stride failed");
        require(std::abs(frame.points[1].x - nose.x) < .005f && std::abs(frame.points[1].y - nose.y) < .005f,
                "Padded row coordinates differ");
        std::vector<uint8_t> black(320 * 320 * 4, 0);
        tracker.reset_tracking();
        require(!tracker.detect_bgra(black.data(), 320, 320, 1280, frame) && !frame.valid, "No-face frame marked valid");
        require(!tracker.detect_bgra(pixels.data(), 320, 320, 100, frame) && !frame.valid, "Invalid stride accepted");
        require(tracker.detect_bgra(pixels.data(), 320, 320, 1280, frame), "Reacquisition failed");
        {
            FaceTracker second;
            require(second.initialize(argv[1], argv[2], 1, error), error.c_str());
            require(second.detect_bgra(pixels.data(), 320, 320, 1280, frame), "Shared runtime second tracker failed");
        }
        require(tracker.detect_bgra(pixels.data(), 320, 320, 1280, frame), "Runtime lifetime failed after second tracker destruction");
        std::cout << "{\"status\":\"pass\",\"landmarks\":478,\"presence\":" << presence
                  << ",\"nose\":[" << nose.x << ',' << nose.y << ',' << nose.z << ']'
                  << ",\"tracking_median_ms\":" << milliseconds[15]
                  << ",\"tracking_p95_ms\":" << milliseconds[28]
                  << ",\"tests\":[\"public_face\",\"finite_coordinates\",\"BGRA_orientation\",\"timestamp\",\"padded_stride\",\"no_face\",\"invalid_stride\",\"reacquisition\",\"shared_runtime_lifetime\"]}\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "tracker_test failed: " << e.what() << '\n'; return 1;
    }
}
