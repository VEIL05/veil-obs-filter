// Replaceable adapter for the official RVM/BMv2 model tensor contracts.
#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace veil {
struct MatteFrame {
    unsigned width=0,height=0;
    std::uint64_t sequence=0,captured_ns=0,generation=0;
    std::vector<std::uint8_t> pixels;
};
class MattingModel {
public:
    MattingModel(const std::filesystem::path&runtime,const std::filesystem::path&model,bool calibrated,bool gpu);
    ~MattingModel();
    MattingModel(const MattingModel&)=delete;
    MattingModel&operator=(const MattingModel&)=delete;
    MatteFrame process(const MatteFrame&source,const MatteFrame*background=nullptr);
    void reset();
    bool gpu()const;
private:
    struct Impl;std::unique_ptr<Impl>impl;
};
}
