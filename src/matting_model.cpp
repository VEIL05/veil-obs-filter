// Official model inference only; no reimplementation/training or frame upload.
// RVM v1.0.0 GPL-3.0; BackgroundMattingV2 v1.0.0 MIT. See THIRD-PARTY-NOTICES.
#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#include "matting_model.h"
#include "matting_pixels.h"
#include "matting_processing.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include <stdexcept>

namespace veil {
namespace {
struct Runtime {HMODULE module=nullptr;std::filesystem::path path;~Runtime(){if(module)FreeLibrary(module);}};
std::mutex runtime_mutex;std::weak_ptr<Runtime>runtime_weak;
std::shared_ptr<Runtime> runtime(const std::filesystem::path&path){
    std::lock_guard<std::mutex>lock(runtime_mutex);
    const auto full=std::filesystem::weakly_canonical(path);
    if(auto r=runtime_weak.lock()){if(r->path!=full)throw std::runtime_error("Different matting runtime already loaded");return r;}
    auto r=std::make_shared<Runtime>();r->path=full;
    r->module=LoadLibraryExW(full.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!r->module)throw std::runtime_error("Matting runtime load failed: "+std::to_string(GetLastError()));
    auto fn=reinterpret_cast<const OrtApiBase*(ORT_API_CALL*)()>(GetProcAddress(r->module,"OrtGetApiBase"));
    if(!fn||!fn()->GetApi(ORT_API_VERSION))throw std::runtime_error("Matting runtime API mismatch");
    Ort::InitApi(fn()->GetApi(ORT_API_VERSION));runtime_weak=r;return r;
}
void planar(const MatteFrame&f,std::vector<float>&v){
    const size_t n=size_t(f.width)*f.height;v.resize(n*3);
    planar_bgra(f.pixels.data(),n,v.data());
}
}
struct MattingModel::Impl {
    std::shared_ptr<Runtime>rt;Ort::Env env{nullptr};Ort::Session session{nullptr};
    Ort::MemoryInfo memory{nullptr};std::vector<Ort::Value>states;
    std::vector<float>source,background;float zero=0,ratio=.375f;
    bool calibrated=false,use_gpu=false;unsigned width=0,height=0;
    Impl(const std::filesystem::path&r,const std::filesystem::path&m,bool c,bool gpu):rt(runtime(r)),calibrated(c){
        env=Ort::Env(ORT_LOGGING_LEVEL_WARNING,"VEIL matting");env.DisableTelemetryEvents();
        Ort::SessionOptions options;options.SetIntraOpNumThreads(4);options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        if(gpu){
            using Append=OrtStatus*(ORT_API_CALL*)(OrtSessionOptions*,int);
            auto append=reinterpret_cast<Append>(GetProcAddress(rt->module,"OrtSessionOptionsAppendExecutionProvider_DML"));
            if(!append)throw std::runtime_error("DirectML provider export is absent");
            options.DisableMemPattern();options.SetExecutionMode(ORT_SEQUENTIAL);Ort::ThrowOnError(append(options,0));use_gpu=true;
        }
        session=Ort::Session(env,m.c_str(),options);memory=Ort::MemoryInfo::CreateCpu(OrtArenaAllocator,OrtMemTypeDefault);
        if(session.GetInputCount()!=(c?2:6)||session.GetOutputCount()<(c?2:6))throw std::runtime_error("Unexpected matting model contract");
        reset();
    }
    void reset(){states.clear();width=height=0;}
    MatteFrame run(const MatteFrame&f,const MatteFrame*b){
        if(f.width<32||f.height<32||f.width>3840||f.height>2160||f.pixels.size()!=size_t(f.width)*f.height*4)throw std::runtime_error("Invalid matting frame");
        if(calibrated&&(!b||b->width!=f.width||b->height!=f.height||b->pixels.size()!=f.pixels.size()))throw std::runtime_error("Empty background reference size mismatch");
        if(width!=f.width||height!=f.height){reset();width=f.width;height=f.height;}
        planar(f,source);std::array<int64_t,4>shape{1,3,f.height,f.width},one{1,1,1,1};std::array<int64_t,1>scalar{1};
        auto src=Ort::Value::CreateTensor<float>(memory,source.data(),source.size(),shape.data(),shape.size());
        std::vector<Ort::Value>in;in.push_back(std::move(src));
        const char*bnames[]={"src","bgr"};const char*rnames[]={"src","r1i","r2i","r3i","r4i","downsample_ratio"};
        const char*bout[]={"pha","fgr"};const char*rout[]={"fgr","pha","r1o","r2o","r3o","r4o"};
        if(calibrated){planar(*b,background);in.push_back(Ort::Value::CreateTensor<float>(memory,background.data(),background.size(),shape.data(),shape.size()));}
        else{
            ratio=std::min(1.f,480.f/std::max(f.width,f.height));
            if(states.empty())for(int i=0;i<4;i++)states.push_back(Ort::Value::CreateTensor<float>(memory,&zero,1,one.data(),one.size()));
            for(auto&s:states)in.push_back(std::move(s));states.clear();
            in.push_back(Ort::Value::CreateTensor<float>(memory,&ratio,1,scalar.data(),scalar.size()));
        }
        std::vector<Ort::Value>out;
        if(use_gpu&&!calibrated){
            // Keep the four recurrent states on DirectML. Only display RGB and
            // alpha need to return to CPU; rebinding also refreshes source data.
            Ort::IoBinding binding(session);
            for(size_t i=0;i<in.size();i++)binding.BindInput(rnames[i],in[i]);
            Ort::MemoryInfo dml("DML",OrtDeviceAllocator,0,OrtMemTypeDefault);
            for(size_t i=0;i<6;i++)binding.BindOutput(rout[i],i<2?memory:dml);
            binding.SynchronizeInputs();session.Run(Ort::RunOptions{nullptr},binding);
            binding.SynchronizeOutputs();out=binding.GetOutputValues();
        }else out=session.Run(Ort::RunOptions{nullptr},calibrated?bnames:rnames,in.data(),in.size(),calibrated?bout:rout,calibrated?2:6);
        const auto&alpha=out[calibrated?0:1];const auto&foreground=out[calibrated?1:0];
        const size_t n=size_t(f.width)*f.height;
        if(alpha.GetTensorTypeAndShapeInfo().GetElementCount()!=n||foreground.GetTensorTypeAndShapeInfo().GetElementCount()!=3*n)throw std::runtime_error("Invalid matting output shape");
        const auto*a=alpha.GetTensorData<float>(),*rgb=foreground.GetTensorData<float>();
        MatteFrame result=f;
        compose_matte(f.pixels.data(),a,rgb,n,result.pixels.data());
        if(!calibrated)for(size_t i=2;i<6;i++)states.push_back(std::move(out[i]));
        return result;
    }
};
MattingModel::MattingModel(const std::filesystem::path&r,const std::filesystem::path&m,bool c,bool gpu):impl(std::make_unique<Impl>(r,m,c,gpu)){}
MattingModel::~MattingModel()=default;
MatteFrame MattingModel::process(const MatteFrame&f,const MatteFrame*b){return impl->run(f,b);}
void MattingModel::reset(){impl->reset();}
bool MattingModel::gpu()const{return impl->use_gpu;}
}
