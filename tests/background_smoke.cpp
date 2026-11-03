// Real upstream DLLs + VEIL properties + GPU pixels, public/generated fixtures.
// Isolated libobs process: no user's OBS profile, camera, microphone or network.
#define main veil_beauty_smoke_entry_unused
#include "gpu_smoke.cpp"
#undef main
#include "../src/video_diagnostics.h"

namespace {
void canary_render(void*p,gs_effect_t*){obs_source_skip_video_filter(static_cast<obs_source_t*>(p));}
void*canary_create(obs_data_t*,obs_source_t*s){return s;}
void canary_destroy(void*){}
void sleep_frame(){std::this_thread::sleep_for(std::chrono::milliseconds(34));}
void diagnostic_observer_canary(){
 std::vector<uint8_t>pixels((64*4+32)*64,0);obs_source_frame frame{};
 frame.data[0]=pixels.data();frame.linesize[0]=64*4+32;frame.width=frame.height=64;frame.format=VIDEO_FORMAT_BGRA;
 veil::VideoDiagnostics trace;require(trace.observe(&frame)==&frame,"Diagnostics must return the identical frame");
 auto unchanged=pixels;trace.observe(&frame);require(pixels==unchanged&&trace.source_changes==0,"Diagnostics must not modify pixels or count an unchanged frame as changed");
 // Packed BGRA must sample across the full image, not only its left quarter.
 for(unsigned y=16;y<48;y++)for(unsigned x=16;x<48;x++)for(int c=0;c<4;c++)pixels[y*frame.linesize[0]+x*4+c]=180;
 unchanged=pixels;trace.observe(&frame);require(trace.source_changes==1&&pixels==unchanged,"Diagnostics must see central motion without changing pixels");
 for(unsigned y=0;y<64;y++)for(unsigned x=64*4;x<frame.linesize[0];x++)pixels[y*frame.linesize[0]+x]=255;
 trace.observe(&frame);require(trace.source_changes==1,"Padding changes must not masquerade as video motion");
 const auto now=os_gettime_ns();trace.source_seen=now+1000000;require(!trace.waiting_for_input(now),"A concurrently newer input clock must not underflow into a stale frame");
 trace.source_seen=now-1000000000ULL;require(trace.waiting_for_input(now),"A one-second input gap must be recognized");
 const auto epoch=trace.source_epoch.load();frame.timestamp=1;trace.observe(&frame);require(trace.source_epoch>epoch&&!trace.waiting_for_input(os_gettime_ns()),"Resuming frames must create a fresh input epoch");
 std::cout<<"PASS diagnostic observer preserves frame identity/pixels and detects central motion without padding"<<std::endl;
}
}
int main(int argc,char**argv){
 if(argc==2&&std::string(argv[1])=="--diagnostics-only"){
  try{diagnostic_observer_canary();return 0;}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
 }
 if(argc!=5&&argc!=7&&argc!=8&&argc!=9)return 2;
 const fs::path root=fs::u8path(argv[1]),obs=fs::u8path(argv[2]),out=fs::u8path(argv[3]),fixture=fs::u8path(argv[4]);
 bool started=false;obs_source_t*source=nullptr,*beauty=nullptr,*canary=nullptr;gs_texrender_t*target=nullptr;gs_stagesurf_t*stage=nullptr;
 try{
  diagnostic_observer_canary();
  fs::create_directories(out);base_set_log_handler(log_handler,nullptr);AddDllDirectory((obs/L"bin/64bit").c_str());
  const bool inference_fault=argc==9&&std::string(argv[8])=="--inference-failure";
  const bool gap_test=argc==9&&std::string(argv[8])=="--input-gap";
  const bool fault_test=argc==9&&(std::string(argv[8])=="--model-failure"||inference_fault);
  if(argc==9&&!fault_test&&!gap_test)return 2;
  if(fault_test)require(fs::weakly_canonical(root)==fs::weakly_canonical(out/"payload"),"Failure test may modify only its private output/payload copy");
  async_fixture=argc>=8;const int soak_frames=async_fixture?std::stoi(argv[7]):0;
  input_width=argc>=7?unsigned(std::stoi(argv[5])):320;input_height=argc>=7?unsigned(std::stoi(argv[6])):320;
  require(input_width>=320&&input_width<=1920&&input_height>=180&&input_height<=1080,"dimensions");
  const unsigned side=std::min(input_width,input_height),left=(input_width-side)/2,top=(input_height-side)/2;
  auto pixel=[&](int x,int y){return (size_t(top+unsigned(y)*side/320)*input_width+left+unsigned(x)*side/320)*4;};
  require(obs_startup("en-US",(out/"config").generic_u8string().c_str(),nullptr),"libobs startup");started=true;
  auto assets=(obs/"data/libobs").generic_u8string()+"/";obs_add_data_path(assets.c_str());
  auto graphics=(obs/"bin/64bit/libobs-d3d11.dll").generic_u8string();obs_video_info v{};v.graphics_module=graphics.c_str();v.fps_num=30;v.fps_den=1;v.base_width=v.output_width=input_width;v.base_height=v.output_height=input_height;v.output_format=VIDEO_FORMAT_BGRA;v.colorspace=VIDEO_CS_709;v.range=VIDEO_RANGE_FULL;
  require(obs_reset_video(&v)==OBS_VIDEO_SUCCESS,"D3D11 startup");
  obs_module_t*m=nullptr;require(obs_open_module(&m,(root/"bin/64bit/obs-trueitem-beauty.dll").generic_u8string().c_str(),(root/"data").generic_u8string().c_str())==MODULE_SUCCESS&&obs_init_module(m),"VEIL module/engines load");
  auto*ai_defaults=obs_get_source_defaults("live_backgroundremoval_lite");auto*key_defaults=obs_get_source_defaults("advanced_masks_filter_v2");require(ai_defaults&&key_defaults,"Bundled engines registered");obs_data_release(ai_defaults);obs_data_release(key_defaults);
  std::vector<std::uint8_t> original(320*320*4);std::ifstream f(fixture,std::ios::binary);f.read((char*)original.data(),original.size());require(f.gcount()==std::streamsize(original.size()),"public fixture");
  input_pixels.assign(size_t(input_width)*input_height*4,0);
  for(unsigned y=0;y<input_height;++y)for(unsigned x=0;x<input_width;++x){auto i=(size_t(y)*input_width+x)*4;input_pixels[i+3]=255;
   if(x>=left&&x<left+side&&y>=top&&y<top+side)std::memcpy(input_pixels.data()+i,original.data()+((size_t((y-top)*320/side)*320+(x-left)*320/side)*4),4);}
  obs_source_info info{};info.id="veil_background_fixture";info.type=OBS_SOURCE_TYPE_INPUT;info.output_flags=async_fixture?OBS_SOURCE_ASYNC_VIDEO:(OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW);info.get_name=fixture_name;info.create=fixture_create;info.destroy=fixture_destroy;info.get_width=fixture_width;info.get_height=fixture_height;info.video_render=async_fixture?nullptr:fixture_render;obs_register_source(&info);
  source=obs_source_create_private(info.id,"Public NASA fixture",nullptr);obs_source_inc_active(source);obs_source_inc_showing(source);
  auto*d=neutral();beauty=obs_source_create_private(filter_id,"VEIL",d);obs_data_release(d);require(beauty,"beauty create");obs_source_filter_add(source,beauty);
  obs_source_info ci{};ci.id="veil_background_canary";ci.type=OBS_SOURCE_TYPE_FILTER;ci.output_flags=OBS_SOURCE_VIDEO;ci.get_name=fixture_name;ci.create=canary_create;ci.destroy=canary_destroy;ci.video_render=canary_render;obs_register_source(&ci);
  d=obs_data_create();obs_data_set_string(d,"canary","untouched");canary=obs_source_create_private(ci.id,"Existing user filter",d);obs_data_release(d);obs_source_filter_add(source,canary);
  obs_enter_graphics();target=gs_texrender_create(GS_BGRA,GS_ZS_NONE);stage=gs_stagesurface_create(input_width,input_height,GS_BGRA);obs_leave_graphics();
  std::vector<double>timing;
  auto capture=[&](int frames){std::vector<std::uint8_t>pixels;for(int n=0;n<frames;++n){auto begin=std::chrono::steady_clock::now();pixels=readback(source,target,stage);timing.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());sleep_frame();}return pixels;};
  auto prepare=[&](){auto*p=obs_source_properties(beauty);require(!obs_properties_get(p,"background_mode")&&!obs_properties_get(p,"background_feather")&&!obs_properties_get(p,"background_apply"),"Main must have no duplicate background controls");require(obs_property_button_clicked(obs_properties_get(p,"background_setup"),beauty),"Create missing background filters callback");obs_properties_destroy(p);};
  auto person_properties=[&](){auto*v=obs_source_get_filter_by_name(source,"VEIL 배경 · 인물");require(v,"Person settings source");auto*p=obs_source_properties(v);obs_source_release(v);return p;};
  auto person_click=[&](obs_properties_t*p,const char*key){auto*v=obs_source_get_filter_by_name(source,"VEIL 배경 · 인물");auto*b=obs_properties_get(p,key);require(b&&obs_property_button_clicked(b,v),"Person property action callback");obs_source_release(v);};
  auto apply=[&](int mode,int feather,int stable=75){
   prepare();
   for(int role:{3,2,1}){auto*v=obs_source_get_filter_by_name(source,role==3?"VEIL 배경 · 인물":role==2?"VEIL 배경 · 가장자리":"VEIL 배경 · 크로마키");require(v,"Prepared helper");
    auto*d=obs_data_create();if(role==3){obs_data_set_int(d,"mode",mode==3?3:1);obs_data_set_int(d,"stability",stable);}
    else if(role==2)obs_data_set_double(d,"mask_feather_size",feather);
    auto*p=obs_source_properties(v);obs_properties_apply_settings(p,d);obs_source_update(v,d);obs_properties_destroy(p);obs_data_release(d);
    obs_source_set_enabled(v,role==3?(mode==1||mode==3):role==2?(mode!=0&&feather>0):mode==2);obs_source_release(v);
   }
  };
  if(!fault_test&&!gap_test){
   // Upgrade an actual legacy AdvancedMasks helper, preserving rendered pixels,
   // fractional settings, enabled state and unrelated sources.
   const auto original_input=input_pixels;auto alpha_fixture=input_pixels;
   for(unsigned y=0;y<input_height;y++)for(unsigned x=0;x<input_width;x++){
    const size_t i=(size_t(y)*input_width+x)*4;const unsigned a=std::min(255u,x*255/std::max(1u,input_width/3));
    for(int c=0;c<3;c++)alpha_fixture[i+c]=uint8_t(unsigned(alpha_fixture[i+c])*a/255);alpha_fixture[i+3]=uint8_t(a);
   }
   replace_fixture(alpha_fixture);d=obs_get_source_defaults("advanced_masks_filter_v2");
   obs_data_set_string(d,"veil_background_owner",obs_source_get_uuid(beauty));obs_data_set_int(d,"veil_background_role",2);
   obs_data_set_int(d,"mask_type",7);obs_data_set_int(d,"mask_effect",1);obs_data_set_double(d,"mask_feather_size",7.25);
   auto*legacy=obs_source_create_private("advanced_masks_filter_v2","VEIL 배경 · 가장자리",d);obs_data_release(d);require(legacy,"Legacy feather creation");obs_source_filter_add(source,legacy);
   auto before=capture(8);auto*p=obs_source_properties(beauty);obs_properties_destroy(p);
   auto*edge_source=obs_source_get_filter_by_name(source,"VEIL 배경 · 가장자리");require(edge_source&&std::string(obs_source_get_id(edge_source))=="veil_background_finish","Legacy finishing role must migrate to managed helper");
   auto after=capture(8);auto difference=metrics(after,before);require(difference.alpha_mae==0&&difference.rgb_mae==0,"Managed helper changes legacy feather pixels");
   d=obs_source_get_settings(edge_source);require(obs_data_get_double(d,"mask_feather_size")==7.25,"Fractional legacy feather must remain exact in edge UI");obs_data_release(d);
   p=obs_source_properties(edge_source);require(obs_properties_get(p,"mask_feather_size")&&!obs_properties_get(p,"background_feather"),"Edge filter owns its single live slider");obs_properties_destroy(p);
   const auto beauty_index=obs_source_filter_get_index(source,beauty);obs_source_filter_remove(source,beauty);
   p=obs_source_properties(edge_source);require(obs_properties_get(p,"mask_feather_size"),"Orphaned helper must remain editable");obs_properties_destroy(p);
   obs_source_filter_add(source,beauty);obs_source_filter_set_index(source,beauty,size_t(beauty_index));
   obs_source_set_enabled(edge_source,false);d=obs_data_create();obs_data_set_int(d,"background_feather",0);obs_data_set_int(d,"background_controls_version",0);obs_source_update(beauty,d);obs_data_release(d);
   p=obs_source_properties(beauty);obs_properties_destroy(p);d=obs_source_get_settings(beauty);require(obs_data_get_double(d,"background_feather")==0&&!obs_source_enabled(edge_source),"Migration must not revive explicitly disabled feather from its stale saved radius");obs_data_release(d);
   // Direct UI edit must change the GPU output with no beauty Apply call.
   obs_source_set_enabled(edge_source,true);d=obs_data_create();obs_data_set_double(d,"mask_feather_size",0);obs_source_update(edge_source,d);obs_data_release(d);
   auto direct_zero=capture(8);require(metrics(direct_zero,after).alpha_mae>0,"Direct feather edit must affect rendered alpha");
   d=obs_data_create();obs_data_set_double(d,"mask_feather_size",7.25);obs_source_update(edge_source,d);obs_data_release(d);require(metrics(capture(8),after).alpha_mae==0,"Direct feather edit must restore rendered alpha");
   obs_source_release(edge_source);obs_source_release(legacy);apply(0,1);replace_fixture(original_input);std::cout<<"PASS legacy feather migration exact pixels/7.25 setting, direct edit changes GPU output and editable orphan"<<std::endl;
  }
  const auto baseline=capture(4);apply(1,1);auto ai=capture(95);save_png(out/"ai-removal.png",ai,input_width,input_height);
  size_t clear=0,solid=0;for(size_t i=3;i<ai.size();i+=4){clear+=ai[i]<30;solid+=ai[i]>225;}
  for(size_t i=0;i<ai.size();i+=4)for(int c=0;c<3;++c)require(ai[i+c]<=ai[i+3]+2,"AI edge must obey premultiplied RGBA (no bright fringe)");
  require(clear>input_width*input_height*.15&&solid>side*side*.15,"AI must remove background and keep subject");require(ai[pixel(145,65)+3]>200,"AI must preserve public subject face");
  require(obs_source_filter_count(source)==5,"Expected beauty, canary, person, edge and disabled chroma");apply(1,1);require(obs_source_filter_count(source)==5,"Repeated apply must not duplicate filters");
  {auto*video=obs_source_get_filter_by_name(source,"VEIL 배경 · 인물");auto*p=obs_source_properties(video);
   for(const char*k:{"stability","device","refresh","restart"})require(obs_properties_get(p,k)&&obs_property_visible(obs_properties_get(p,k)),"Person owns its visible live controls");
   require(!obs_properties_get(p,"mask_feather_size"),"Person must not duplicate edge controls");
   require(!obs_properties_get(p,"help")&&std::string(obs_property_long_description(obs_properties_get(p,"stability"))).size()>0,"Person UI keeps details in optional help");
   require(!obs_properties_get(p,"mode")&&!obs_properties_get(p,"capture")&&!obs_properties_get(p,"view")&&!obs_properties_get(p,"forget")&&!obs_properties_get(p,"reference_preview"),"Product exposes automatic only, no retired registration controls");obs_properties_destroy(p);
   const auto index=obs_source_filter_get_index(source,beauty);obs_source_filter_remove(source,beauty);p=obs_source_properties(video);require(obs_property_visible(obs_properties_get(p,"stability")),"Orphan person controls remain editable");obs_properties_destroy(p);obs_source_filter_add(source,beauty);obs_source_filter_set_index(source,beauty,size_t(index));obs_source_release(video);}
  if(!fault_test&&!gap_test){
   auto*person=obs_source_get_filter_by_name(source,"VEIL 배경 · 인물");auto*edge=obs_source_get_filter_by_name(source,"VEIL 배경 · 가장자리");
   d=obs_data_create();obs_data_set_int(d,"mode",3);obs_data_set_int(d,"stability",31);obs_source_update(person,d);obs_data_release(d);
   d=obs_data_create();obs_data_set_double(d,"mask_feather_size",7.25);obs_source_update(edge,d);obs_data_release(d);capture(4);
   auto*person_settings=obs_source_get_settings(person);const auto retry=obs_data_get_int(person_settings,"retry");obs_data_release(person_settings);
   const std::string uuid=obs_source_get_uuid(person);const auto person_index=obs_source_filter_get_index(source,person),edge_index=obs_source_filter_get_index(source,edge);const auto count=obs_source_filter_count(source);
   d=obs_data_create();obs_data_set_int(d,"background_mode",1);obs_data_set_int(d,"background_stability",75);obs_data_set_double(d,"background_feather",1);obs_source_update(beauty,d);obs_data_release(d);
   prepare();prepare();capture(4);
   d=obs_source_get_settings(person);require(obs_data_get_int(d,"mode")==1&&obs_data_get_int(d,"stability")==31&&obs_data_get_int(d,"retry")==retry,"Prepare must not overwrite direct person edits or restart model");obs_data_release(d);
   d=obs_source_get_settings(edge);require(obs_data_get_double(d,"mask_feather_size")==7.25,"Prepare must retain direct fractional edge edit");obs_data_release(d);
   require(uuid==obs_source_get_uuid(person)&&obs_source_filter_count(source)==count&&obs_source_filter_get_index(source,person)==person_index&&obs_source_filter_get_index(source,edge)==edge_index,"Prepare must preserve UUID/count/order");
   auto save=[&](const char*name){auto*path=obs_data_create();const auto filename=(out/name).generic_u8string();obs_data_set_string(path,"preset_save",filename.c_str());obs_source_update(beauty,path);obs_data_release(path);auto*p=obs_source_properties(beauty);require(obs_property_button_clicked(obs_properties_get(p,"preset_export"),beauty),"Export callback");obs_properties_destroy(p);return obs_data_create_from_json_file(filename.c_str());};
   auto*preset=save("direct-background.json");auto*bg=obs_data_get_obj(preset,"background");require(obs_data_get_int(bg,"mode")==1&&obs_data_get_int(bg,"stability")==31&&obs_data_get_double(bg,"feather")==7.25,"Export must read actual child values, not stale beauty fields");obs_data_release(bg);obs_data_release(preset);
   obs_source_set_enabled(edge,false);preset=save("edge-disabled.json");bg=obs_data_get_obj(preset,"background");require(obs_data_get_double(bg,"feather")==0,"Disabled edge exports effective zero");obs_data_release(bg);obs_data_release(preset);prepare();require(!obs_source_enabled(edge),"Prepare must not reenable a disabled edge");
   d=obs_source_get_settings(edge);require(obs_data_get_double(d,"mask_feather_size")==7.25,"Export must not erase disabled edge radius");obs_data_release(d);
   auto import=[&](const fs::path&file){auto*path=obs_data_create();obs_data_set_string(path,"preset_load",file.generic_u8string().c_str());obs_source_update(beauty,path);obs_data_release(path);auto*p=obs_source_properties(beauty);require(obs_property_button_clicked(obs_properties_get(p,"preset_import"),beauty),"Import callback");obs_properties_destroy(p);capture(3);};
   d=obs_data_create();obs_data_set_int(d,"stability",6);obs_data_set_int(d,"device",1);obs_source_update(person,d);obs_data_release(d);import(out/"direct-background.json");
   d=obs_source_get_settings(person);require(obs_data_get_int(d,"stability")==31&&obs_data_get_int(d,"mode")==1&&obs_data_get_int(d,"device")==1,"Background preset applies directly while preserving local CPU choice");obs_data_release(d);require(obs_source_enabled(edge),"Background preset restores enabled feather");
   d=obs_data_create();obs_data_set_int(d,"stability",44);obs_data_set_int(d,"device",0);obs_source_update(person,d);obs_data_release(d);
   import(root/"data/user-preset.json");d=obs_source_get_settings(person);require(obs_data_get_int(d,"stability")==44&&obs_data_get_int(d,"mode")==1,"Face-only old preset must preserve live background settings");obs_data_release(d);
   d=neutral();obs_source_update(beauty,d);obs_data_release(d);obs_source_release(person);obs_source_release(edge);apply(1,1);
   std::cout<<"PASS direct-edit ownership, prepare idempotence, child snapshot export, background import and face-only preservation"<<std::endl;
  }
  if(gap_test){
   for(int cycle=0;cycle<2;cycle++){
    async_feed_paused=true;capture(40);
    auto*video=obs_source_get_filter_by_name(source,"VEIL 배경 · 인물");require(video,"Matting filter for input gap");
    auto*p=obs_source_properties(video);const std::string waiting=obs_property_description(obs_properties_get(p,"status"));obs_properties_destroy(p);obs_source_release(video);
    require(waiting.find("카메라에서 새 영상")!=std::string::npos,"Stalled input must be reported separately from model errors");
    require(status(beauty).find("카메라에서 새 영상")!=std::string::npos,"Beauty must report waiting for input");
    blog(LOG_INFO,"[VEIL TEST] input pause settled %d",cycle);
    const auto raw=capture(320);require(metrics(raw,input_pixels).rgb_mae<1&&metrics(raw,input_pixels).alpha_mae==0,"Stalled input must bypass processing to original RGB/alpha");
    blog(LOG_INFO,"[VEIL TEST] input resume %d",cycle);
    async_timestamp_offset=os_gettime_ns()-1000000000ULL;async_feed_paused=false;
    auto resumed=capture(100);require(resumed[3]<30&&resumed[pixel(145,65)+3]>200,"New camera timestamp epoch must recover cutout without Apply or restart");
   }
   std::ofstream(out/"input-gap.json")<<"{\"passed\":true,\"pause_resume_cycles\":2,\"source_timestamp_reset\":true,\"original_passthrough\":true,\"automatic_cutout_resume\":true}\n";
   std::cout<<"PASS input pause uses direct original passthrough; timestamp reset automatically resumes cutout in two cycles"<<std::endl;
  }else if(fault_test){
   auto*video=obs_source_get_filter_by_name(source,"VEIL 배경 · 인물");require(video,"Initial working model");
   obs_source_filter_remove(source,video);obs_source_release(video);
   const auto model=root/"data/engines/video/data/models/rvm.onnx",held=out/"temporarily-held-rvm.onnx";
   fs::rename(model,held);
   try{
    if(inference_fault)fs::copy_file(out/"invalid-shape-model.onnx",model);
    apply(1,0);capture(35);
    video=obs_source_get_filter_by_name(source,"VEIL 배경 · 인물");require(video,"Failed model filter must remain available");
    auto*p=obs_source_properties(video);const std::string failed_status=obs_property_description(obs_properties_get(p,"status"));obs_properties_destroy(p);obs_source_release(video);
    require(failed_status.find("원본 영상을 표시")!=std::string::npos,"Real model-load failure must report original-video fallback");
    auto dimmed=input_pixels;for(size_t i=0;i<dimmed.size();i+=4)for(int c=0;c<3;c++)dimmed[i+c]=uint8_t(dimmed[i+c]*.55);
    replace_fixture(dimmed);const auto dark=capture(15);const auto dark_metrics=metrics(dark,dimmed);
    require(dark_metrics.alpha_mae==0&&dark_metrics.rgb_mae<1,"Model failure must preserve fresh original camera RGB/alpha");
    replace_fixture(input_pixels);const auto bright=capture(15);const auto bright_metrics=metrics(bright,input_pixels);
    require(bright_metrics.alpha_mae==0&&bright_metrics.rgb_mae<1&&metrics(bright,dark).rgb_mae>5,"Input must keep progressing after failure without source or OBS restart");
    std::cout<<"PASS real "<<(inference_fault?"inference-shape":"model-load")<<" error falls back to continuously changing original RGB/alpha without input restart"<<std::endl;
    if(inference_fault)fs::remove(model);fs::rename(held,model);apply(1,0);const auto recovered=capture(100);
    require(recovered[3]<30&&recovered[pixel(145,65)+3]>200,"Restored model must resume cutout on the same input without OBS restart");
    std::ofstream(out/"fault-recovery.json")<<"{\"passed\":true,\"fault\":\""<<(inference_fault?"valid model; GPU and CPU Run reject frame shape":"missing model; GPU and CPU load fail")<<"\",\"original_rgb_mae_dark\":"<<dark_metrics.rgb_mae<<",\"original_rgb_mae_bright\":"<<bright_metrics.rgb_mae<<",\"original_alpha_mae\":0,\"same_input_recovered\":true,\"scope\":\"private payload copy and public async fixture only; no real camera or OBS configuration\"}\n";
    std::cout<<"PASS restoring model and applying settings recovers cutout on the same async input"<<std::endl;
   }catch(...){std::error_code ec;if(fs::exists(held)){fs::remove(model,ec);ec.clear();fs::rename(held,model,ec);}throw;}
  }else{
  unsigned motion_face_min=255;double static_alpha_change=0;auto previous=ai;
  for(int n=0;n<24;n++){
   auto moving=input_pixels;const int dx=int(std::lround(std::sin(n*.25)*12));
   for(unsigned y=0;y<input_height;y++)for(unsigned x=0;x<input_width;x++){
    const int sx=std::clamp(int(x)-dx,0,int(input_width)-1);size_t dst=(size_t(y)*input_width+x)*4,src=(size_t(y)*input_width+sx)*4;
    for(int c=0;c<3;c++){const int noise=int((x*17+y*7+n*11+c*3)%7)-3;moving[dst+c]=std::uint8_t(std::clamp(int(input_pixels[src+c]*(.82+.18*std::cos(n*.17)))+noise,0,255));}
   }
   replace_fixture(moving);auto current=capture(3);motion_face_min=std::min(motion_face_min,unsigned(current[pixel(145,65)+3]));
   if(n>0)static_alpha_change+=std::abs(int(current[pixel(145,65)+3])-int(previous[pixel(145,65)+3]));previous=std::move(current);
  }
  require(motion_face_min>230,"Moving/noisy/dimmed public face core must remain opaque");std::cout<<"Motion/noise face min alpha="<<motion_face_min<<" variation="<<static_alpha_change<<"\n";
  replace_fixture(input_pixels);ai=capture(12);
  if(async_fixture){
   auto dimmed=input_pixels;for(size_t i=0;i<dimmed.size();i+=4)for(int c=0;c<3;c++)dimmed[i+c]=std::uint8_t(dimmed[i+c]*.6);
   auto face_brightness=[&](const std::vector<std::uint8_t>&pixels){double sum=0;unsigned count=0;
    for(int y=55;y<75;y++)for(int x=135;x<155;x++){auto i=pixel(x,y);require(pixels[i+3]>200,"Progression canary must retain face coverage");for(int c=0;c<3;c++){sum+=pixels[i+c];count++;}}return sum/count;};
   auto*matting=obs_source_get_filter_by_name(source,"VEIL 배경 · 인물");require(matting,"Matting filter for eye-toggle canary");
   for(auto*filter:{beauty,matting})for(int cycle=0;cycle<2;cycle++){
    obs_source_set_enabled(filter,false);replace_fixture(dimmed);auto dark=capture(18);
    obs_source_set_enabled(filter,true);replace_fixture(input_pixels);auto bright=capture(24);
    require(face_brightness(bright)-face_brightness(dark)>15,"Enabling VEIL must continue displaying fresh RGB frames");
    replace_fixture(dimmed);dark=capture(18);
    require(face_brightness(bright)-face_brightness(dark)>15,"Enabled VEIL must track subsequent RGB changes");
   }
   obs_source_release(matting);replace_fixture(input_pixels);ai=capture(24);
   std::cout<<"PASS fresh RGB progression after beauty/matting eye toggles (4 cycles, 8 measured changes)"<<std::endl;
  }
  if(soak_frames){
   auto empty=input_pixels;for(size_t i=0;i<empty.size();i+=4)for(int c=0;c<3;c++)empty[i+c]=110;
   unsigned minimum=255;int missing=0;
   for(int n=0;n<soak_frames;n++){
    const bool absent=n>=soak_frames/3&&n<soak_frames/3+60;
    if(n==soak_frames/3)replace_fixture(empty);if(n==soak_frames/3+60)replace_fixture(input_pixels);
    auto current=capture(1);
    // A second view of an asynchronous source exercises the filter dialog path.
    readback(source,target,stage);
    if(!absent&&(n<soak_frames/3||n>soak_frames/3+120)){minimum=std::min(minimum,unsigned(current[pixel(145,65)+3]));missing+=current[pixel(145,65)+3]<180;}
    if(n%300==0)std::cout<<"Async soak frame="<<n<<" face-min="<<minimum<<" missing="<<missing<<std::endl;
   }
   require(missing==0,"Continuous asynchronous source must not lose a returning face");
   std::cout<<"PASS async soak frames="<<soak_frames<<" minimum-face-alpha="<<minimum<<std::endl;
   replace_fixture(input_pixels);ai=capture(24);
  }
  {auto*reference_props=person_properties();const bool reference_build=obs_properties_get(reference_props,"capture")!=nullptr;obs_properties_destroy(reference_props);if(reference_build){
  // New fixed-background path: actual VEIL buttons, real model, explicit plate
  // capture, persistence, and deletion. Plate and person are public/generated.
  std::vector<std::uint8_t>wall(ai.size());
  for(size_t i=0;i<wall.size();i+=4){wall[i]=105;wall[i+1]=112;wall[i+2]=120;wall[i+3]=255;}
  auto composite=wall;for(size_t i=0;i<wall.size();i+=4)for(int c=0;c<3;c++)composite[i+c]=std::uint8_t(std::min(255u,unsigned(ai[i+c])+unsigned(wall[i+c])*(255-ai[i+3])/255));
  replace_fixture(wall);apply(3,0);auto*p=person_properties();
  require(std::string(obs_property_description(obs_properties_get(p,"reference_preview"))).find("data:image/png")==std::string::npos,"No reference preview before registration");
  require((person_click(p,"capture"),true),"VEIL plate capture button");obs_properties_destroy(p);
  capture(140); // Three-second countdown and one-second averaged plate.
  replace_fixture(composite);auto calibrated=capture(65);
  p=person_properties();person_click(p,"view");
  const std::string registered_preview=obs_property_description(obs_properties_get(p,"reference_preview"));
  require(registered_preview.find("data:image/png;base64,")!=std::string::npos,"View registered background must contain a real PNG");
  std::ofstream(out/"registered-background-preview.html")<<registered_preview; // public/generated plate only
  const std::string plate_status=obs_property_description(obs_properties_get(p,"status"));std::cout<<plate_status<<"\n";obs_properties_destroy(p);
  require(plate_status.find("등록한 사진으로 윤곽 다듬는 중")!=std::string::npos&&plate_status.find("그래픽카드")!=std::string::npos&&plate_status.find("사진 저장됨")!=std::string::npos,"Registered mode must really use the plate with recurrent matting on GPU and persist it");
  require(calibrated[3]<10&&calibrated[pixel(145,65)+3]>230,"Registered cleanup must remove known wall and keep face core");
  for(size_t i=0;i<calibrated.size();i+=4)for(int c=0;c<3;c++)require(calibrated[i+c]<=calibrated[i+3]+2,"BMv2 output must be premultiplied");
  save_png(out/"calibrated.png",calibrated,input_width,input_height);
  size_t plates=0;for(auto&entry:fs::recursive_directory_iterator(out/"config"))plates+=entry.path().extension()==".veilbg";require(plates==1,"Exactly one local plate must be saved");
  // Recreate with the saved UUID: the registered plate must remain usable.
  auto*video=obs_source_get_filter_by_name(source,"VEIL 배경 · 인물");require(video,"Video engine source");
  auto*saved_video=obs_save_source(video);obs_source_filter_remove(source,video);obs_source_release(video);
  video=obs_load_source(saved_video);obs_data_release(saved_video);require(video,"Reload calibrated source");obs_source_filter_add(source,video);
  capture(50);auto*vp=obs_source_properties(video);const std::string reload_status=obs_property_description(obs_properties_get(vp,"status"));obs_properties_destroy(vp);
  vp=obs_source_properties(video);require(std::string(obs_property_description(obs_properties_get(vp,"reference_preview"))).find("data:image/png;base64,")!=std::string::npos,"Preview must survive source recreation");obs_properties_destroy(vp);
  require(reload_status.find("등록한 사진으로 윤곽 다듬는 중")!=std::string::npos&&reload_status.find("사진 저장됨")!=std::string::npos,"Registered plate must survive filter recreation");
  // A failed overwrite must remain visible and must not leave a reference temp.
  fs::path plate_file;for(auto&entry:fs::recursive_directory_iterator(out/"config"))if(entry.path().extension()==".veilbg")plate_file=entry.path();
  require(!plate_file.empty()&&SetFileAttributesW(plate_file.c_str(),FILE_ATTRIBUTE_READONLY),"Set isolated plate read-only");
  replace_fixture(wall);p=person_properties();person_click(p,"capture");obs_properties_destroy(p);capture(145);
  p=person_properties();person_click(p,"view");
  const std::string storage_status=obs_property_description(obs_properties_get(p,"status"));obs_properties_destroy(p);
  require(storage_status.find("저장하거나 지우지 못했습니다")!=std::string::npos,"Storage failure must survive later inference status updates");
  auto temporary=plate_file;temporary+=L".tmp";require(!fs::exists(temporary),"Failed reference write must remove its temporary image");
  require(SetFileAttributesW(plate_file.c_str(),FILE_ATTRIBUTE_NORMAL),"Restore isolated reference permissions");
  std::cout<<"PASS reference storage error remains visible; no temporary image left"<<std::endl;
  p=person_properties();person_click(p,"forget");
  require(std::string(obs_property_description(obs_properties_get(p,"reference_preview"))).find("data:image/png")==std::string::npos,"Deleted reference must no longer have a preview");obs_properties_destroy(p);capture(45);
  vp=obs_source_properties(video);const std::string fallback_status=obs_property_description(obs_properties_get(vp,"status"));obs_properties_destroy(vp);obs_source_release(video);
  require(fallback_status.find("자동으로 제거하고 있습니다")!=std::string::npos,"Deleting plate must fall back to RVM");
  video=obs_source_get_filter_by_name(source,"VEIL 배경 · 인물");const std::string person_uuid=obs_source_get_uuid(video);
  auto*cpu=obs_data_create();obs_data_set_int(cpu,"device",1);obs_source_update(video,cpu);obs_data_release(cpu);capture(65);
  d=obs_data_create();obs_data_set_int(d,"background_controls_version",0);obs_source_update(beauty,d);obs_data_release(d);
  p=obs_source_properties(beauty);obs_properties_destroy(p);d=obs_source_get_settings(video);require(obs_data_get_int(d,"device")==1,"Opening main must preserve local child CPU choice");obs_data_release(d);
  require(person_uuid==obs_source_get_uuid(video),"Main controls migration must preserve person UUID and photo identity");
  vp=obs_source_properties(video);const std::string cpu_status=obs_property_description(obs_properties_get(vp,"status"));std::cout<<cpu_status<<"\n";obs_properties_destroy(vp);require(cpu_status.find("프로세서")!=std::string::npos,"CPU fallback backend must produce a result");obs_source_release(video);
  plates=0;for(auto&entry:fs::recursive_directory_iterator(out/"config"))plates+=entry.path().extension()==".veilbg";require(plates==0,"Delete plate must remove stored reference");
  }}
  for(auto&entry:fs::recursive_directory_iterator(out/"config"))require(entry.path().extension()!=".veilbg","Automatic-only product must not create reference files");
  std::vector<std::uint8_t>green(size_t(input_width)*input_height*4);for(unsigned y=0;y<input_height;++y)for(unsigned x=0;x<input_width;++x){
   const size_t i=(size_t(y)*input_width+x)*4;const float r=std::hypot((float(x)*320/input_width-160)/87,(float(y)*320/input_height-170)/115);
   green[i]=std::uint8_t(12+(x*17+y*7)%13);green[i+1]=std::uint8_t(225+(x*11+y*3)%25);green[i+2]=std::uint8_t(10+(x+y)%10);green[i+3]=255;
   if(r<1){green[i]=80;green[i+1]=120;green[i+2]=190;}
  }
  replace_fixture(green);apply(2,0);auto key=capture(10);save_png(out/"chroma-key.png",key,input_width,input_height);
  require(key[3]<5&&key[pixel(160,170)+3]>245,"Chroma must remove noisy green and keep subject");
  {auto*current=obs_source_get_filter_by_name(source,"VEIL 배경 · 크로마키");auto*s=obs_source_get_settings(current);
   auto*legacy=obs_source_create_private("advanced_masks_filter_v2","Legacy chroma comparison",s);obs_data_release(s);require(legacy,"Legacy chroma comparison creation");
   const auto index=obs_source_filter_get_index(source,current);obs_source_set_enabled(current,false);obs_source_filter_add(source,legacy);obs_source_filter_set_index(source,legacy,size_t(index));
   const auto legacy_pixels=capture(8);auto error=metrics(legacy_pixels,key);require(error.alpha_mae==0&&error.rgb_mae==0,"Managed chroma changes legacy AdvancedMasks pixels");
   obs_source_filter_remove(source,legacy);obs_source_release(legacy);obs_source_set_enabled(current,true);obs_source_release(current);
   std::cout<<"PASS native managed chroma and original AdvancedMasks output match exactly"<<std::endl;}
  apply(2,2);auto feather=capture(10);save_png(out/"chroma-feather.png",feather,input_width,input_height);
  size_t edge=0,key_edge=0;for(size_t i=3;i<feather.size();i+=4){edge+=feather[i]>5&&feather[i]<250;key_edge+=key[i]>5&&key[i]<250;}
  require(edge>key_edge,"Feather must produce a softer subpixel boundary");
  require(obs_source_filter_count(source)==5,"Switching modes preserves one instance per owned engine");
  // Enabling face correction after keying must retain coverage. No opaque halo.
  d=obs_data_create();obs_data_set_int(d,"smooth",80);obs_data_set_int(d,"glow",30);obs_data_set_int(d,"bright",20);obs_source_update(beauty,d);obs_data_release(d);
  auto beautified=capture(12);require(beautified[3]<5&&beautified[pixel(160,170)+3]>245,"Beauty after key must preserve background coverage");save_png(out/"chroma-beauty.png",beautified,input_width,input_height);
  d=neutral();obs_source_update(beauty,d);obs_data_release(d);apply(0,1);auto off=capture(4);require(metrics(off,green).alpha_mae==0&&metrics(off,green).rgb_mae==0,"Background off must restore full exact fixture");
  d=obs_source_get_settings(canary);require(std::string(obs_data_get_string(d,"canary"))=="untouched"&&obs_source_enabled(canary),"Existing filter must remain intact");obs_data_release(d);
  d=obs_save_source(source);require(d,"save source");obs_data_save_json(d,(out/"public-fixture-source.json").generic_u8string().c_str());
  obs_source_dec_active(source);obs_source_dec_showing(source);obs_source_release(beauty);obs_source_release(canary);obs_source_release(source);source=nullptr;beauty=canary=nullptr;
  input_pixels=green;source=obs_load_source(d);obs_data_release(d);require(source,"source reload");obs_source_inc_active(source);obs_source_inc_showing(source);
  beauty=obs_source_get_filter_by_name(source,"VEIL");canary=obs_source_get_filter_by_name(source,"Existing user filter");require(beauty&&canary,"saved filters reload");
  apply(2,2);require(obs_source_filter_count(source)==5,"Reload must preserve owner UUID and reuse filters");auto reloaded=capture(10);require(reloaded[3]<5&&reloaded[pixel(160,170)+3]>245,"Reloaded chroma output");
  std::sort(timing.begin(),timing.end());double p95=timing[timing.size()*95/100];
  std::ofstream(out/"results.json")<<"{\"passed\":true,\"width\":"<<input_width<<",\"height\":"<<input_height<<",\"capture_pipeline_p95_ms\":"<<p95<<",\"ai_clear_pixels\":"<<clear<<",\"ai_solid_pixels\":"<<solid<<",\"key_transition_pixels\":"<<key_edge<<",\"feather_transition_pixels\":"<<edge<<",\"scope\":\"isolated libobs + actual upstream DLLs + public/generated fixtures\"}\n";
  std::cout<<"PASS selectable AI/chroma/feather, beauty alpha, mode switch, duplicate prevention, existing filter canary; AI clear="<<clear<<" solid="<<solid<<" edge "<<key_edge<<" -> "<<edge<<"\n";
  }
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
 if(source){obs_source_dec_active(source);obs_source_dec_showing(source);}if(beauty)obs_source_release(beauty);if(canary)obs_source_release(canary);if(source)obs_source_release(source);
 obs_enter_graphics();gs_texrender_destroy(target);gs_stagesurface_destroy(stage);obs_leave_graphics();if(started)obs_shutdown();return 0;
}
