// VEIL native model adapter. GPL-3.0-or-later.
// Consumes OBS frames; no camera handle, audio, network or Python runtime.
#include "matting_model.h"
#include "reference_preview.h"
#include "background_refinement.h"
#include "managed_background.h"
#include "video_diagnostics.h"
#include <obs-module.h>
#include <util/platform.h>
#include <util/bmem.h>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <fstream>
#include <mutex>
#include <thread>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char*obs_module_description(void){return "VEIL recurrent human matting and registered-background cleanup";}
namespace {
using veil::MatteFrame;namespace fs=std::filesystem;
struct Filter {
    obs_source_t*source=nullptr;std::mutex mutex;std::condition_variable cv;std::thread worker;
    bool stop=false,queued=false,save=false,erase=false,configured=false,busy=false,reference_saved=false;int mode=1,stability=75,device=0;long long retry=0;
    uint64_t generation=1,sequence=0,uploaded=0,last_capture=0,dropped=0,completed=0;
    uint64_t failed_generation=0,input_epoch=0,last_input_frame=0;bool input_waiting=false;
    unsigned width=0,height=0;MatteFrame pending;
    std::shared_ptr<MatteFrame>result,reference;std::string status="배경 제거를 준비하고 있습니다. 잠시만 기다려 주세요.";
    std::string preview="아직 등록한 배경이 없습니다. 카메라 화면을 비운 뒤 빈 배경을 등록해 주세요.";
    std::string storage_error;
    fs::path file;double capture_start=0,capture_end=0;unsigned capture_count=0;
    std::vector<uint32_t>accumulator;
    gs_texrender_t*raw=nullptr;gs_stagesurf_t*stage=nullptr;gs_texture_t*texture=nullptr;
    unsigned tex_width=0,tex_height=0;double latency_ms=0;
    veil::VideoDiagnostics trace;
};
struct WorkerBusyScope{Filter*f;~WorkerBusyScope(){std::lock_guard<std::mutex>lock(f->mutex);f->busy=false;}};
fs::path data_path(const char*name){char*p=obs_module_file(name);fs::path r=p?fs::u8path(p):fs::path{};bfree(p);return r;}
double seconds(){return double(os_gettime_ns())/1e9;}
bool valid_size(unsigned w,unsigned h){return w>=32&&h>=32&&w<=3840&&h<=2160;}
std::shared_ptr<MatteFrame>load_reference(const fs::path&file){
    std::ifstream in(file,std::ios::binary);char magic[8]{};uint32_t size[2]{};in.read(magic,8);in.read((char*)size,8);
    if(!in||std::string(magic,8)!="VEILBG01"||!valid_size(size[0],size[1]))return {};
    auto f=std::make_shared<MatteFrame>();f->width=size[0];f->height=size[1];f->pixels.resize(size_t(size[0])*size[1]*4);
    in.read((char*)f->pixels.data(),f->pixels.size());if(!in||in.peek()!=EOF)return {};return f;
}
void save_reference(const fs::path&file,const MatteFrame&f){
    fs::create_directories(file.parent_path());auto temp=file;temp+=L".tmp";
    try{
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);uint32_t size[]={f.width,f.height};out.write("VEILBG01",8);out.write((char*)size,8);out.write((char*)f.pixels.data(),f.pixels.size());if(!out)throw std::runtime_error("reference write failed");}
    if(!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("reference replacement failed");
    }catch(...){std::error_code ec;fs::remove(temp,ec);throw;}
}
std::string make_preview(const MatteFrame&frame){
    try{return veil::reference_preview_html(frame);}catch(const std::exception&e){blog(LOG_WARNING,"[VEIL matting] %s",e.what());return "등록한 배경의 미리보기를 준비하지 못했습니다. 빈 배경을 다시 등록해 주세요.";}
}
std::string preview(Filter*f){if(!f)return "배경을 등록하면 이곳에서 확인할 수 있습니다.";std::lock_guard<std::mutex>lock(f->mutex);return f->preview;}
void worker(Filter*f){
    std::unique_ptr<veil::MattingModel>model;uint64_t active_generation=0;
    std::shared_ptr<MatteFrame>previous_source,previous_output;
    std::shared_ptr<MatteFrame>previewed;
    unsigned empty_results=0;
    while(true){
        MatteFrame frame;std::shared_ptr<MatteFrame>reference;int mode,stability,device;bool save=false,erase=false;
        {std::unique_lock<std::mutex>lock(f->mutex);f->cv.wait_for(lock,std::chrono::seconds(1),[&]{return f->stop||f->queued||f->save||f->erase;});
            // Complete the user's pending reference write/delete before joining
            // at shutdown. Inference work is disposable; registered data is not.
            if(f->stop&&!f->save&&!f->erase)break;
            if(!f->queued&&!f->save&&!f->erase){lock.unlock();f->trace.beat("matting",f->source);continue;}
            reference=f->reference;save=f->save;erase=f->erase;f->save=f->erase=false;
            if(f->queued&&!f->stop){frame=std::move(f->pending);if(frame.generation==f->failed_generation)frame.pixels.clear();}f->queued=false;f->busy=!frame.pixels.empty();mode=f->mode;stability=f->stability;device=f->device;}
        WorkerBusyScope busy_scope{f};
        try{if(erase){std::error_code ec;fs::remove(f->file,ec);if(ec)throw std::runtime_error("reference deletion failed");}
            if(save&&reference)save_reference(f->file,*reference);
            if(save||erase){std::lock_guard<std::mutex>lock(f->mutex);f->storage_error.clear();if(save&&f->reference==reference)f->reference_saved=true;}
        }catch(const std::exception&e){std::lock_guard<std::mutex>lock(f->mutex);f->storage_error="등록한 배경을 저장하거나 지우지 못했습니다. 저장 폴더의 쓰기 권한을 확인해 주세요.";blog(LOG_WARNING,"[VEIL matting] %s",e.what());}
        if(reference&&reference!=previewed){auto html=make_preview(*reference);std::lock_guard<std::mutex>lock(f->mutex);if(f->reference==reference)f->preview=std::move(html);previewed=reference;}
        if(!reference)previewed.reset();
        if(frame.pixels.empty())continue;
        veil::WorkTraceScope work_trace(f->trace,frame.sequence);f->trace.beat("matting",f->source);
        const bool registered=mode==3&&reference&&reference->width==frame.width&&reference->height==frame.height;
        try{
            if(!model||active_generation!=frame.generation){
                model.reset();previous_source.reset();previous_output.reset();empty_results=0;
                const auto runtime=data_path("../bin/64bit/veil-matting-ort.dll"),path=data_path("models/rvm.onnx");
                try{model=std::make_unique<veil::MattingModel>(runtime,path,false,device!=1);}
                catch(const std::exception&e){f->trace.errors++;if(device==1)throw;blog(LOG_WARNING,"[VEIL matting] DirectML unavailable, CPU fallback: %s",e.what());model=std::make_unique<veil::MattingModel>(runtime,path,false,false);}
                active_generation=frame.generation;
                blog(LOG_INFO,"[VEIL matting] RVM%s on %s, %ux%u",registered?" + registered background":"",model->gpu()?"DirectML":"CPU",frame.width,frame.height);
            }
            MatteFrame processed;
            try{processed=model->process(frame);}
            catch(const std::exception&e){
                f->trace.errors++;if(!model->gpu())throw;blog(LOG_WARNING,"[VEIL matting] GPU inference failed, retrying on CPU: %s",e.what());model.reset();
                model=std::make_unique<veil::MattingModel>(data_path("../bin/64bit/veil-matting-ort.dll"),data_path("models/rvm.onnx"),false,false);
                processed=model->process(frame);
            }
            auto result=std::make_shared<MatteFrame>(std::move(processed));
            unsigned samples=0,input_alpha=0,input_rgb=0,output_alpha=0;
            for(size_t i=0;i<result->pixels.size();i+=256){samples++;input_alpha+=frame.pixels[i+3];output_alpha+=result->pixels[i+3];for(int c=0;c<3;c++)input_rgb+=frame.pixels[i+c];}
            if(samples&&output_alpha<samples){
                empty_results++;
                if(empty_results==30||empty_results%300==0)blog(LOG_INFO,"[VEIL matting] Empty foreground %u outputs; input alpha %.1f, input RGB %.1f; RVM%s %ux%u generation %llu",empty_results,double(input_alpha)/samples,double(input_rgb)/(3*samples),registered?" + registered background":"",frame.width,frame.height,(unsigned long long)frame.generation);
            }else{if(empty_results>=30)blog(LOG_INFO,"[VEIL matting] Foreground recovered after %u empty outputs",empty_results);empty_results=0;}
            auto automatic_history=registered?std::make_shared<MatteFrame>():result;
            const bool reference_used=veil::postprocess_matte(frame,registered?reference.get():nullptr,*result,
                previous_source.get(),previous_output.get(),stability,registered?automatic_history.get():nullptr);
            previous_source=std::make_shared<MatteFrame>(std::move(frame));previous_output=automatic_history;
            {std::lock_guard<std::mutex>lock(f->mutex);if(result->generation!=f->generation)continue;
                f->result=result;f->completed++;
                f->status=reference_used?"등록한 사진으로 윤곽 다듬는 중":mode==3?"현재 영상과 배경 사진이 맞지 않아 자동으로 제거하고 있습니다. 카메라 위치와 조명을 확인해 주세요.":"자동 배경 제거 중";
                if(empty_results>=30)f->status="현재 인물을 찾지 못하고 있습니다. 화면에 있는데도 보이지 않으면 '배경 설정 적용'을 다시 눌러 주세요.";
                f->status+=model->gpu()?" · 그래픽카드 사용":" · 프로세서 사용";
            }
        }catch(const std::exception&e){
            f->trace.errors++;
            blog(LOG_WARNING,"[VEIL matting] Inference failed: %s",e.what());
            std::lock_guard<std::mutex>lock(f->mutex);if(frame.generation==f->generation){f->failed_generation=frame.generation;f->result.reset();f->status="배경 제거에 문제가 생겨 원본 영상을 표시합니다. VEIL 필터에서 '배경 설정 적용'을 눌러 다시 시도해 주세요.";}
        }
    }
}
void update(void*data,obs_data_t*d){auto*f=static_cast<Filter*>(data);std::lock_guard<std::mutex>lock(f->mutex);
    const int mode=obs_data_get_int(d,"mode")==3?3:1,device=obs_data_get_int(d,"device")==1?1:0;const auto retry=obs_data_get_int(d,"retry");
    const bool changed=!f->configured||mode!=f->mode||device!=f->device||retry!=f->retry;
    f->configured=true;f->mode=mode;f->stability=std::clamp(int(obs_data_get_int(d,"stability")),0,100);f->device=device;f->retry=retry;
    if(changed){f->generation++;f->queued=false;f->result.reset();f->status="배경 제거를 준비하고 있습니다. 잠시만 기다려 주세요.";}
    // OBS defers video-source updates until its next render. A capture button
    // may arrive before that update; applying calibrated mode must retain it.
    if(mode!=3){f->capture_start=f->capture_end=0;f->accumulator.clear();f->capture_count=0;}
}
std::string status(Filter*f){
    if(!f)return "배경 제거 방식을 선택한 뒤 적용해 주세요.";std::lock_guard<std::mutex>lock(f->mutex);std::string s=f->status;
    if(f->input_waiting)s="카메라에서 새 영상이 들어오기를 기다리고 있습니다. 영상이 멈춰 있으면 카메라 속성에서 비활성화한 뒤 다시 활성화해 주세요.";
    const double now=seconds();if(f->capture_start>now)s=std::to_string(int(std::ceil(f->capture_start-now)))+"초 후 배경을 등록합니다. 카메라 화면에서 나와 주세요.";
    else if(f->capture_end>now)s="배경을 등록하고 있습니다. 1초 동안 화면을 비워 주세요.";
    else if(f->reference){s+=f->reference_saved?"\n배경 사진 저장됨 · ":"\n배경 사진 저장 중 · ";s+=std::to_string(f->reference->width)+"×"+std::to_string(f->reference->height);
        if(f->width&&(f->width!=f->reference->width||f->height!=f->reference->height))s+="\n현재 영상과 크기가 달라 자동 제거를 사용합니다. 저장한 사진은 보관되며, 원래 영상 크기로 돌아가면 다시 사용합니다.";}
    if(f->completed)s+="\n영상 처리 지연: "+std::to_string(int(std::lround(f->latency_ms)))+"ms";
    if(f->latency_ms>100)s+="\n영상 처리가 지연되고 있습니다. 카메라 해상도를 낮추거나 그래픽카드 사용 상태를 확인해 주세요.";
    if(!f->storage_error.empty())s+="\n"+f->storage_error;
    return s;
}
bool action(obs_properties_t*p,obs_property_t*property,void*opaque){
    auto*f=static_cast<Filter*>(opaque);if(!f)return false;const std::string name=obs_property_name(property);
    auto*settings=obs_source_get_settings(f->source);const bool calibrated=obs_data_get_int(settings,"mode")==3;obs_data_release(settings);
    {std::lock_guard<std::mutex>lock(f->mutex);
        if(name=="capture"){
            if(!calibrated||f->capture_end>seconds())return false;
            f->capture_start=seconds()+3;f->capture_end=f->capture_start+1;f->capture_count=0;f->accumulator.clear();
        }else if(name=="forget"){
            f->capture_start=f->capture_end=0;f->accumulator.clear();f->capture_count=0;f->reference.reset();f->reference_saved=false;f->preview="등록한 배경을 지웠습니다. 새 배경을 등록하면 이곳에서 확인할 수 있습니다.";f->erase=true;f->generation++;f->result.reset();f->cv.notify_one();
        }
    }
    const auto s=status(f),html=preview(f);obs_property_set_description(obs_properties_get(p,"status"),s.c_str());obs_property_set_description(obs_properties_get(p,"reference_preview"),html.c_str());return true;
}
obs_properties_t*properties(void*data){
    auto*f=static_cast<Filter*>(data);auto*p=obs_properties_create();
    auto*m=obs_properties_add_list(p,"mode","배경 제거 방식",OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(m,"자동 배경 제거",1);obs_property_list_add_int(m,"배경 등록 후 제거",3);
    obs_properties_add_int_slider(p,"stability","가장자리 떨림 줄이기",0,100,1);
    auto*d=obs_properties_add_list(p,"device","영상 처리 방식",OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);obs_property_list_add_int(d,"자동 선택 · 그래픽카드 우선",0);obs_property_list_add_int(d,"프로세서 사용",1);
    obs_properties_add_text(p,"help","등록 버튼을 누른 뒤 3초 안에 화면에서 나와 1초 동안 비워 주세요. 배경은 이 PC에만 저장됩니다. 카메라 위치·조명·영상 크기를 바꾸면 다시 등록해 주세요.",OBS_TEXT_INFO);
    for(const auto&b:{std::pair{"capture","빈 배경 등록 (3초 후)"},std::pair{"refresh","처리 상태 확인"},std::pair{"forget","등록한 배경 지우기"}})obs_properties_add_button2(p,b.first,b.second,action,f);
    const auto s=status(f),html=preview(f);obs_properties_add_text(p,"status",s.c_str(),OBS_TEXT_INFO);obs_properties_add_text(p,"reference_preview",html.c_str(),OBS_TEXT_INFO);
    if(f&&veil_is_managed(f->source)){
        for(const char*key:{"mode","stability","device","help","capture","refresh","forget","reference_preview"})obs_property_set_visible(obs_properties_get(p,key),false);
        obs_properties_add_text(p,"managed_help","이 효과는 VEIL 필터에서 관리합니다.\n설정·배경 등록·사진 보기는 ‘VEIL 필터 → 배경 제거’에서 사용할 수 있습니다.\n이 목록에서 눈을 끄면 인물 배경 제거가 꺼집니다.",OBS_TEXT_INFO);
    }
    return p;
}
void defaults(obs_data_t*d){obs_data_set_default_int(d,"mode",1);obs_data_set_default_int(d,"stability",75);obs_data_set_default_int(d,"device",0);}
void destroy(void*data){auto*f=static_cast<Filter*>(data);if(!f)return;
    {std::lock_guard<std::mutex>lock(f->mutex);f->stop=true;}f->cv.notify_one();if(f->worker.joinable())f->worker.join();
    obs_enter_graphics();gs_texrender_destroy(f->raw);gs_stagesurface_destroy(f->stage);gs_texture_destroy(f->texture);obs_leave_graphics();delete f;
}
void*create(obs_data_t*d,obs_source_t*s){auto*f=new Filter;f->source=s;
    try{std::string uuid=obs_source_get_uuid(s);if(uuid.empty()||uuid.size()>64||uuid.find_first_not_of("0123456789abcdefABCDEF-")!=std::string::npos)throw std::runtime_error("Invalid filter UUID");
        char*p=obs_module_config_path("backgrounds");if(!p)throw std::runtime_error("No module config directory");f->file=fs::u8path(p)/(uuid+".veilbg");bfree(p);f->reference=load_reference(f->file);if(f->reference){f->reference_saved=true;f->preview=make_preview(*f->reference);}
        update(f,d);obs_enter_graphics();f->raw=gs_texrender_create(GS_BGRA,GS_ZS_NONE);obs_leave_graphics();f->worker=std::thread(worker,f);return f;
    }catch(const std::exception&e){blog(LOG_ERROR,"[VEIL matting] Create failed: %s",e.what());destroy(f);return nullptr;}
}
void submit(Filter*f,MatteFrame&&in){
    const double now=seconds();std::lock_guard<std::mutex>lock(f->mutex);
    if(f->capture_start>0&&now>=f->capture_start){
        if(f->accumulator.size()!=in.pixels.size()){f->accumulator.assign(in.pixels.size(),0);f->capture_count=0;}
        if(now<f->capture_end){for(size_t i=0;i<in.pixels.size();i++)f->accumulator[i]+=in.pixels[i];f->capture_count++;}
        else{
            if(f->capture_count>=3){auto reference=std::make_shared<MatteFrame>(in);for(size_t i=0;i<in.pixels.size();i++)reference->pixels[i]=uint8_t(f->accumulator[i]/f->capture_count);
                f->reference=reference;f->reference_saved=false;f->preview="등록한 배경을 준비하고 있습니다. 잠시 후 '등록한 배경 보기'를 눌러 주세요.";f->save=true;f->generation++;f->result.reset();f->status="배경 등록을 마쳤습니다.";}
            else f->status="배경을 충분히 등록하지 못했습니다. 카메라 영상을 확인한 뒤 다시 등록해 주세요.";
            f->accumulator.clear();f->capture_start=f->capture_end=0;f->capture_count=0;
        }
    }
    if(f->queued)f->dropped++;in.generation=f->generation;f->pending=std::move(in);f->queued=true;f->cv.notify_one();
}
void render(void*data,gs_effect_t*){
    auto*f=static_cast<Filter*>(data);veil::RenderTraceScope render_trace(f->trace);auto*target=obs_filter_get_target(f->source);
    const auto now=os_gettime_ns();const bool asynchronous=f->trace.async_input(f->source);
    const uint64_t input_frame=asynchronous?f->trace.source_frames.load():0,epoch=f->trace.source_epoch.load();
    bool bypass=false,ready=false;
    {std::lock_guard<std::mutex>lock(f->mutex);
        const bool waiting=asynchronous&&f->trace.waiting_for_input(now);
        if((waiting&&!f->input_waiting)||(asynchronous&&epoch!=f->input_epoch)){
            f->generation++;f->queued=false;f->result.reset();f->last_input_frame=0;
            if(!waiting)f->status="배경 제거를 준비하고 있습니다. 잠시만 기다려 주세요.";
        }
        f->input_waiting=waiting;f->input_epoch=epoch;bypass=waiting||f->failed_generation==f->generation;ready=!f->busy&&!f->queued;
    }
    // Use OBS's direct passthrough before any readback after a terminal error
    // or input interruption. Reprocessing a cached frame cannot reopen input.
    if(bypass){obs_source_skip_video_filter(f->source);return;}
    const unsigned w=target?obs_source_get_base_width(target):0,h=target?obs_source_get_base_height(target):0;
    if(!valid_size(w,h)||!f->raw){obs_source_skip_video_filter(f->source);return;}
    f->trace.phase=2;if(!obs_source_process_filter_begin(f->source,GS_BGRA,OBS_NO_DIRECT_RENDERING)){f->trace.begins_failed++;return;}
    const bool linear=gs_set_linear_srgb(false),srgb=gs_framebuffer_srgb_enabled();gs_enable_framebuffer_srgb(false);gs_blend_state_push();gs_blend_function(GS_BLEND_ONE,GS_BLEND_ZERO);
    gs_texrender_reset(f->raw);
    if(!gs_texrender_begin(f->raw,w,h)){f->trace.targets_failed++;gs_blend_state_pop();gs_enable_framebuffer_srgb(srgb);gs_set_linear_srgb(linear);obs_source_process_filter_end(f->source,obs_get_base_effect(OBS_EFFECT_DEFAULT),w,h);return;}
    vec4 clear{};gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,float(w),0,float(h),-100,100);
    f->trace.phase=3;obs_source_process_filter_end(f->source,obs_get_base_effect(OBS_EFFECT_DEFAULT),w,h);gs_texrender_end(f->raw);auto*raw=gs_texrender_get_texture(f->raw);
    if(f->width!=w||f->height!=h){const bool resized=f->width!=0;gs_stagesurface_destroy(f->stage);f->stage=gs_stagesurface_create(w,h,GS_BGRA);f->width=w;f->height=h;
        std::lock_guard<std::mutex>lock(f->mutex);f->generation++;f->queued=false;f->result.reset();if(resized){f->capture_start=f->capture_end=0;f->accumulator.clear();f->capture_count=0;}}
    if(f->stage&&ready&&(asynchronous?input_frame!=f->last_input_frame:now-f->last_capture>=33000000ULL)){
        f->trace.phase=4;
        MatteFrame in;in.width=w;in.height=h;in.captured_ns=now;in.sequence=++f->sequence;in.pixels.resize(size_t(w)*h*4);
        gs_stage_texture(f->stage,raw);uint8_t*data=nullptr;uint32_t stride=0;
        if(gs_stagesurface_map(f->stage,&data,&stride)){
            f->trace.capture(data,stride,w,h);
            for(unsigned y=0;y<h;y++)std::copy_n(data+size_t(y)*stride,w*4,in.pixels.data()+size_t(y)*w*4);gs_stagesurface_unmap(f->stage);submit(f,std::move(in));f->last_input_frame=input_frame;}
        else f->trace.maps_failed++;
        f->last_capture=now;
    }
    f->trace.phase=5;std::shared_ptr<MatteFrame>result;{std::lock_guard<std::mutex>lock(f->mutex);result=f->result;}
    bool valid=result&&result->width==w&&result->height==h&&now-result->captured_ns<1000000000ULL;
    if(valid&&result->sequence!=f->uploaded){
        if(f->tex_width!=w||f->tex_height!=h){gs_texture_destroy(f->texture);const uint8_t*p=result->pixels.data();f->texture=gs_texture_create(w,h,GS_BGRA,1,&p,GS_DYNAMIC);f->tex_width=w;f->tex_height=h;}
        else gs_texture_set_image(f->texture,result->pixels.data(),w*4,false);f->uploaded=result->sequence;
        {std::lock_guard<std::mutex>lock(f->mutex);f->latency_ms=(os_gettime_ns()-result->captured_ns)/1e6;}
    }
    gs_blend_state_pop();auto*e=obs_get_base_effect(OBS_EFFECT_DEFAULT);auto*image=valid&&f->texture?f->texture:raw;
    gs_effect_set_texture(gs_effect_get_param_by_name(e,"image"),image);while(gs_effect_loop(e,"Draw"))gs_draw_sprite(image,0,w,h);
    gs_enable_framebuffer_srgb(srgb);gs_set_linear_srgb(linear);
}
const char*name(void*){return "VEIL 배경 제거";}
obs_source_frame*observe_frame(void*data,obs_source_frame*frame){return static_cast<Filter*>(data)->trace.observe(frame);}
void tick(void*data,float){auto*f=static_cast<Filter*>(data);f->trace.ticks++;f->trace.beat("matting",f->source);}
}
bool obs_module_load(void){obs_source_info i{};i.id="veil_video_matting";i.type=OBS_SOURCE_TYPE_FILTER;i.output_flags=OBS_SOURCE_VIDEO;
    i.get_name=name;i.create=create;i.destroy=destroy;i.get_defaults=defaults;i.get_properties=properties;i.update=update;i.video_render=render;i.filter_video=observe_frame;i.video_tick=tick;obs_register_source(&i);return true;}
