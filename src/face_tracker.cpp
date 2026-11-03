// Detector/crop/tracking algorithm adapted from Lumina Beauty (MIT), commit
// f1472d581dab9706c8536e0decd46592f9045d44, src/engine/LmFaceDetector.cpp.
// Copyright (c) 2026 Lumina Beauty contributors. Full retained license and
// source provenance: third_party/face-tracking/LICENSE-LUMINA.txt, NOTICE.md.
#include "face_tracker.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace trueitem {
namespace {
constexpr int detect_size = 128, landmark_size = 256;
constexpr float roi_expand = 1.5f;
struct Runtime {
    HMODULE module = nullptr;
    std::filesystem::path path;
    ~Runtime() { if (module) FreeLibrary(module); }
};
std::mutex runtime_mutex;
std::weak_ptr<Runtime> runtime_weak;

std::shared_ptr<Runtime> acquire_runtime(const std::filesystem::path& path) {
    const auto full = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
    if (!std::filesystem::is_regular_file(full))
        throw std::runtime_error("ONNX Runtime DLL does not exist");
    std::lock_guard<std::mutex> lock(runtime_mutex);
    if (auto current = runtime_weak.lock()) {
        if (current->path != full)
            throw std::runtime_error("A different ONNX Runtime is already in use");
        return current;
    }
    auto runtime = std::make_shared<Runtime>();
    runtime->path = full;
    // Runtime loading only; no Win32 window/UI enumeration or process access.
    runtime->module = LoadLibraryExW(full.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!runtime->module)
        throw std::runtime_error("Cannot load packaged ONNX Runtime (Windows error " +
                                 std::to_string(GetLastError()) + ")");
    using ApiBaseFn = const OrtApiBase* (ORT_API_CALL*)();
    const auto get_api_base = reinterpret_cast<ApiBaseFn>(
        GetProcAddress(runtime->module, "OrtGetApiBase"));
    if (!get_api_base) throw std::runtime_error("ONNX Runtime API export is absent");
    const auto* api = get_api_base()->GetApi(ORT_API_VERSION);
    if (!api) throw std::runtime_error("Packaged ONNX Runtime API version is incompatible");
    Ort::InitApi(api);
    runtime_weak = runtime;
    return runtime;
}

float sigmoid(float x) {
    if (!std::isfinite(x)) return 0;
    return 1.0f / (1.0f + std::exp(-std::clamp(x, -80.f, 80.f)));
}
struct Roi { float cx = 0, cy = 0, size = 0, rotation = 0; };
bool valid_roi(const Roi& r, int w, int h) {
    const float limit = static_cast<float>(std::max(w, h));
    return std::isfinite(r.cx) && std::isfinite(r.cy) && std::isfinite(r.size)
        && std::isfinite(r.rotation) && r.size > 8.f && r.size < limit * 4.f
        && std::abs(r.cx) < limit * 8.f && std::abs(r.cy) < limit * 8.f;
}
struct PixelFace { std::array<Point3, landmark_count> points{}; float presence = 0; };

struct Model {
    Ort::Session session{nullptr};
    std::string input_name;
    std::vector<std::string> output_names;
    std::vector<const char*> output_ptrs;
    std::vector<Ort::Value> outputs;
    int side = 0;
    void initialize(Ort::Env& env, const std::filesystem::path& path,
                    Ort::SessionOptions& options, int expected_side) {
        session = Ort::Session(env, path.c_str(), options);
        side = expected_side;
        if (session.GetInputCount() != 1) throw std::runtime_error("Unexpected face model input count");
        auto type = session.GetInputTypeInfo(0);
        auto tensor = type.GetTensorTypeAndShapeInfo();
        const auto shape = tensor.GetShape();
        const std::array<int64_t,4> expected{1, side, side, 3};
        bool compatible_shape = shape.size() == expected.size();
        for (size_t i = 0; compatible_shape && i < shape.size(); ++i)
            compatible_shape = shape[i] == -1 || shape[i] == expected[i];
        if (tensor.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || !compatible_shape)
            throw std::runtime_error("Unexpected face model tensor format");
        Ort::AllocatorWithDefaultOptions allocator;
        auto input = session.GetInputNameAllocated(0, allocator);
        input_name = input.get();
        for (size_t i = 0; i < session.GetOutputCount(); ++i) {
            auto output_type = session.GetOutputTypeInfo(i);
            if (output_type.GetTensorTypeAndShapeInfo().GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
                throw std::runtime_error("Unexpected face model output type");
            auto name = session.GetOutputNameAllocated(i, allocator);
            output_names.emplace_back(name.get());
        }
        for (const auto& name : output_names) output_ptrs.push_back(name.c_str());
    }
    void run(float* input, Ort::MemoryInfo& memory) {
        const int64_t shape[] = {1, side, side, 3};
        auto tensor = Ort::Value::CreateTensor<float>(memory, input,
            static_cast<size_t>(side) * side * 3, shape, 4);
        const char* inputs[] = {input_name.c_str()};
        outputs = session.Run(Ort::RunOptions{nullptr}, inputs, &tensor, 1,
                              output_ptrs.data(), output_ptrs.size());
    }
};
}

struct FaceTracker::Impl {
    // Runtime must outlive every Ort C++ object (reverse member destruction).
    std::shared_ptr<Runtime> runtime;
    Ort::Env env;
    Ort::MemoryInfo memory;
    Model detector, landmarks;
    std::vector<float> anchors, detect_input, landmark_input;
    Roi roi;
    bool tracking = false;
    int previous_width = 0, previous_height = 0;

    Impl(std::shared_ptr<Runtime> runtime_, const std::filesystem::path& dir, int threads)
        : runtime(std::move(runtime_)), env(ORT_LOGGING_LEVEL_ERROR, "trueitem-beauty"),
          memory(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
        env.DisableTelemetryEvents();
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(std::clamp(threads, 1, 4));
        options.SetInterOpNumThreads(1);
        options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        options.AddConfigEntry("session.intra_op.allow_spinning", "0");
        options.AddConfigEntry("session.inter_op.allow_spinning", "0");
        detector.initialize(env, dir / "face_detector.onnx", options, detect_size);
        landmarks.initialize(env, dir / "face_landmarks.onnx", options, landmark_size);
        const int grids[] = {16, 8}, per_cell[] = {2, 6};
        for (int layer = 0; layer < 2; ++layer)
            for (int y = 0; y < grids[layer]; ++y)
                for (int x = 0; x < grids[layer]; ++x)
                    for (int n = 0; n < per_cell[layer]; ++n) {
                        anchors.push_back((x + .5f) / grids[layer]);
                        anchors.push_back((y + .5f) / grids[layer]);
                    }
        detect_input.resize(detect_size * detect_size * 3);
        landmark_input.resize(landmark_size * landmark_size * 3);
    }

    // Same rotated square, pixel-centre convention, letterboxing, RGB order and
    // normalization as upstream. Direct BGRA bilinear sampling avoids an
    // unnecessary I420 conversion and its chroma loss; alpha is ignored.
    static void crop(const uint8_t* pixels, int w, int h, int stride, const Roi& r,
                     int side, float scale, float bias, float* dst) {
        const float c = std::cos(r.rotation), s = std::sin(r.rotation);
        const float step = r.size / side;
        for (int v = 0; v < side; ++v) {
            const float ly = (v + .5f - side * .5f) * step;
            for (int u = 0; u < side; ++u) {
                const float lx = (u + .5f - side * .5f) * step;
                const float sx = r.cx + lx * c - ly * s - .5f;
                const float sy = r.cy + lx * s + ly * c - .5f;
                float* out = dst + (static_cast<size_t>(v) * side + u) * 3;
                const int x0 = static_cast<int>(std::floor(sx)), y0 = static_cast<int>(std::floor(sy));
                if (x0 < 0 || y0 < 0 || x0 >= w - 1 || y0 >= h - 1) {
                    out[0] = out[1] = out[2] = bias;
                    continue;
                }
                const float fx = sx - x0, fy = sy - y0;
                const auto* row0 = pixels + static_cast<size_t>(y0) * stride + x0 * 4;
                const auto* row1 = row0 + stride;
                for (int channel = 0; channel < 3; ++channel) {
                    const int bgra_channel = 2 - channel;
                    const float value = (row0[bgra_channel] * (1 - fx) + row0[4 + bgra_channel] * fx) * (1 - fy)
                                      + (row1[bgra_channel] * (1 - fx) + row1[4 + bgra_channel] * fx) * fy;
                    out[channel] = value * scale + bias;
                }
            }
        }
    }

    bool find_face(const uint8_t* pixels, int w, int h, int stride) {
        const Roi full{w * .5f, h * .5f, static_cast<float>(std::max(w, h)), 0};
        crop(pixels, w, h, stride, full, detect_size, 2.f / 255.f, -1.f, detect_input.data());
        detector.run(detect_input.data(), memory);
        const size_t count = anchors.size() / 2;
        const float *boxes = nullptr, *scores = nullptr;
        for (auto& value : detector.outputs) {
            const auto size = value.GetTensorTypeAndShapeInfo().GetElementCount();
            if (size == count * 16) boxes = value.GetTensorData<float>();
            if (size == count) scores = value.GetTensorData<float>();
        }
        if (!boxes || !scores) throw std::runtime_error("Unexpected face detector outputs");
        size_t best = 0; float best_score = 0;
        for (size_t i = 0; i < count; ++i) {
            const float score = sigmoid(scores[i]);
            if (score > best_score) { best_score = score; best = i; }
        }
        if (best_score < .5f) return false;
        const float* b = boxes + best * 16;
        for (int i = 0; i < 8; ++i) if (!std::isfinite(b[i])) return false;
        const float ax = anchors[best * 2], ay = anchors[best * 2 + 1];
        const float ox = full.cx - full.size * .5f, oy = full.cy - full.size * .5f;
        const float eye0x = (b[4] / detect_size + ax) * full.size + ox;
        const float eye0y = (b[5] / detect_size + ay) * full.size + oy;
        const float eye1x = (b[6] / detect_size + ax) * full.size + ox;
        const float eye1y = (b[7] / detect_size + ay) * full.size + oy;
        roi.cx = (b[0] / detect_size + ax) * full.size + ox;
        roi.cy = (b[1] / detect_size + ay) * full.size + oy;
        roi.size = std::max(b[2], b[3]) / detect_size * full.size * roi_expand;
        roi.rotation = std::atan2(eye1y - eye0y, eye1x - eye0x);
        return valid_roi(roi, w, h);
    }

    bool run_landmarks(const uint8_t* pixels, int w, int h, int stride, const Roi& r, PixelFace& face) {
        if (!valid_roi(r, w, h)) return false;
        crop(pixels, w, h, stride, r, landmark_size, 1.f / 255.f, 0, landmark_input.data());
        landmarks.run(landmark_input.data(), memory);
        const float *points = nullptr, *presence = nullptr;
        for (auto& value : landmarks.outputs) {
            const auto size = value.GetTensorTypeAndShapeInfo().GetElementCount();
            if (size == landmark_count * 3) points = value.GetTensorData<float>();
            else if (size == 1 && !presence) presence = value.GetTensorData<float>();
        }
        if (!points || !presence) throw std::runtime_error("Unexpected face landmark outputs");
        face.presence = sigmoid(presence[0]);
        if (face.presence < .5f) return false;
        const float c = std::cos(r.rotation), s = std::sin(r.rotation), step = r.size / landmark_size;
        for (int i = 0; i < landmark_count; ++i) {
            const float lx = (points[i * 3] - landmark_size * .5f) * step;
            const float ly = (points[i * 3 + 1] - landmark_size * .5f) * step;
            face.points[i] = {r.cx + lx * c - ly * s, r.cy + lx * s + ly * c, points[i * 3 + 2] * step};
            const auto& p = face.points[i];
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return false;
        }
        return true;
    }

    static Roi roi_from_face(const PixelFace& face) {
        const auto& left = face.points[33]; const auto& right = face.points[263];
        Roi next;
        next.rotation = std::atan2(right.y - left.y, right.x - left.x);
        const float c = std::cos(-next.rotation), s = std::sin(-next.rotation);
        float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
        for (const auto& p : face.points) {
            const float x = p.x * c - p.y * s, y = p.x * s + p.y * c;
            min_x = std::min(min_x, x); max_x = std::max(max_x, x);
            min_y = std::min(min_y, y); max_y = std::max(max_y, y);
        }
        const float cx = (min_x + max_x) * .5f, cy = (min_y + max_y) * .5f;
        next.cx = cx * c + cy * s;
        next.cy = -cx * s + cy * c;
        next.size = std::max(max_x - min_x, max_y - min_y) * roi_expand;
        return next;
    }

};

FaceTracker::FaceTracker() = default;
FaceTracker::~FaceTracker() = default;
bool FaceTracker::initialize(const std::filesystem::path& model_dir,
                             const std::filesystem::path& runtime_dll_path,
                             int threads, std::string& error) {
    impl_.reset(); error_.clear(); error.clear();
    try {
        auto runtime = acquire_runtime(runtime_dll_path);
        impl_ = std::make_unique<Impl>(std::move(runtime), model_dir, threads);
        return true;
    } catch (const std::exception& e) {
        error_ = e.what(); error = error_; return false;
    }
}
void FaceTracker::reset_tracking() {
    if (impl_) impl_->tracking = false;
}
const std::string& FaceTracker::last_error() const { return error_; }
bool FaceTracker::detect_bgra(const uint8_t* pixels, int w, int h, int stride, LandmarkFrame& frame) {
    const auto timestamp = frame.timestamp_ns;
    frame = {}; frame.timestamp_ns = timestamp; error_.clear();
    if (!impl_) { error_ = "Face tracker is not initialized"; return false; }
    if (!pixels || w < 16 || h < 16 || w > 4096 || h > 4096 || stride < w * 4) {
        reset_tracking(); error_ = "Invalid BGRA frame"; return false;
    }
    try {
        auto& p = *impl_;
        if (w != p.previous_width || h != p.previous_height) {
            reset_tracking(); p.previous_width = w; p.previous_height = h;
        }
        if (!p.tracking) {
            if (!p.find_face(pixels, w, h, stride)) return false;
        }
        PixelFace face;
        if (!p.run_landmarks(pixels, w, h, stride, p.roi, face)) {
            reset_tracking(); return false;
        }
        if (!p.tracking) {
            const Roi refined = Impl::roi_from_face(face);
            PixelFace second;
            if (p.run_landmarks(pixels, w, h, stride, refined, second)) face = second;
        }
        p.roi = Impl::roi_from_face(face);
        if (!valid_roi(p.roi, w, h)) {
            reset_tracking(); return false;
        }
        // The OBS worker applies OneEuro once, after normalized coordinates.
        p.tracking = true;
        for (int i = 0; i < landmark_count; ++i)
            frame.points[i] = {face.points[i].x / w, face.points[i].y / h, face.points[i].z / w};
        frame.presence = face.presence; frame.valid = true;
        return true;
    } catch (const std::exception& e) {
        reset_tracking(); error_ = e.what(); return false;
    }
}
}
