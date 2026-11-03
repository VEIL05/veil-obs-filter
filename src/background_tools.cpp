// Copyright (C) 2026 TRUE ITEM contributors. GPL-2.0-or-later.
// API-only integration of unmodified GPL upstream OBS filter modules.
#include "background_tools.h"
#include <util/bmem.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace veil {
namespace {
constexpr const char* ai_id="live_backgroundremoval_lite";
constexpr const char* key_id="advanced_masks_filter_v2";
constexpr const char* video_id="veil_video_matting";
constexpr const char* finish_id="veil_background_finish";
const char* role_names[]={"VEIL 배경 · AI (이전)","VEIL 배경 · 크로마키","VEIL 배경 · 가장자리","VEIL 배경 · 인물"};

void load_engine(const char*name,const char*relative,const char*data,bool lite){
    if(obs_get_module(name))return;
    char*dll=obs_module_file(relative),*assets=obs_module_file(data);
    if(!dll||!assets){bfree(dll);bfree(assets);return;}
    const auto path=std::filesystem::u8path(dll).generic_u8string();
    const auto dir=std::filesystem::u8path(assets).generic_u8string();bfree(dll);bfree(assets);
    obs_module_t*module=nullptr;
    const int result=obs_open_module(&module,path.c_str(),dir.c_str());
    if(result!=MODULE_SUCCESS||!module){blog(LOG_WARNING,"[VEIL] Background engine %s unavailable (%d)",name,result);return;}
    // Bundled engine updates are delivered by the VEIL installer. These flags
    // affect only a newly loaded bundled Lite module, never an installed one.
    if(lite)for(const char*file:{"live-backgroundremoval-lite_PluginConfig_HasFirstRunOccurred.txt",
                              "live-backgroundremoval-lite_PluginConfig_AutoCheckForUpdateDisabled.txt"}){
        char*p=obs_module_get_config_path(module,file);
        if(p){std::error_code ec;auto flag=std::filesystem::u8path(p);std::filesystem::create_directories(flag.parent_path(),ec);
            if(!ec&&!std::filesystem::exists(flag,ec))std::ofstream(flag)<<"VEIL bundled engine; offline updates via installer\n";bfree(p);}
    }
    if(!obs_init_module(module))blog(LOG_WARNING,"[VEIL] Background engine %s initialization failed",name);
}
bool available(const char*id){auto*d=obs_get_source_defaults(id);const bool found=d!=nullptr;obs_data_release(d);return found;}
struct Owned {std::string owner;obs_source_t*roles[4]{};};
void collect(obs_source_t*,obs_source_t*filter,void*opaque){
    auto&found=*static_cast<Owned*>(opaque);auto*d=obs_source_get_settings(filter);
    if(found.owner==obs_data_get_string(d,"veil_background_owner")){
        const int role=int(obs_data_get_int(d,"veil_background_role"));
        if(role>=0&&role<4&&!found.roles[role])found.roles[role]=obs_source_get_ref(filter);
    }obs_data_release(d);
}
bool apply(obs_properties_t*props,obs_property_t*,void*opaque){
    auto*beauty=static_cast<obs_source_t*>(opaque);if(!beauty)return false;
    auto*parent=obs_filter_get_parent(beauty);
    if(!parent){obs_property_set_description(obs_properties_get(props,"background_result"),"OBS 카메라 소스에 VEIL 필터를 추가한 뒤 다시 적용해 주세요.");return true;}
    auto*d=obs_source_get_settings(beauty);const int mode=std::clamp(int(obs_data_get_int(d,"background_mode")),0,3);
    Owned owned{obs_source_get_uuid(beauty)};obs_source_enum_filters(parent,collect,&owned);
    const bool want[4]={false,mode==2,mode!=0&&obs_data_get_double(d,"background_feather")>0,mode==1||mode==3};
    const char*ids[]={ai_id,finish_id,finish_id,video_id};bool success=true;std::vector<int>created;
    // Prepare all required filters before disabling the previous working mode.
    for(int i=0;i<4;++i)if(want[i]&&!owned.roles[i]){
        if(!available(ids[i])){success=false;break;}
        auto*settings=obs_get_source_defaults(ids[i]);
        obs_data_set_string(settings,"veil_background_owner",owned.owner.c_str());obs_data_set_int(settings,"veil_background_role",i);
        owned.roles[i]=obs_source_create_private(ids[i],role_names[i],settings);obs_data_release(settings);
        if(!owned.roles[i]||!obs_obj_get_data(owned.roles[i])){success=false;break;}
        obs_source_set_enabled(owned.roles[i],false);obs_source_filter_add(parent,owned.roles[i]);created.push_back(i);
    }
    if(success){
        for(int i=0;i<4;++i)if(owned.roles[i]){
            if(want[i]){
                auto*settings=obs_source_get_settings(owned.roles[i]);
                if(i==0){
                    obs_data_set_int(settings,"filterLevel",500);
                    // User-facing stability increases averaging; motion setting
                    // remains separate so reducing flicker is an explicit tradeoff.
                    obs_data_set_double(settings,"timeAveragedFilteringAlpha",.85-std::clamp(obs_data_get_int(d,"background_stability"),0LL,100LL)*.0065);
                    obs_data_set_double(settings,"motionIntensityThresholdPowDb",-60);
                    obs_data_set_int(settings,"blurSize",0);
                    obs_data_set_bool(settings,"advancedSettings",true);
                    obs_data_set_double(settings,"guidedFilterEpsPowDb",-40);
                    obs_data_set_double(settings,"maskGamma",2.5);
                    obs_data_set_double(settings,"maskLowerBoundAmpDb",-25);
                    obs_data_set_double(settings,"maskUpperBoundMarginAmpDb",-25);
                }else if(i==1){
                    obs_data_set_int(settings,"mask_type",6);obs_data_set_int(settings,"mask_effect",1);obs_data_set_int(settings,"key_type",1);
                    obs_data_set_int(settings,"advanced_color_type",1);obs_data_set_int(settings,"advanced_key_color_single",obs_data_get_int(d,"background_color"));
                    obs_data_set_int(settings,"mask_advanced_key_similarity",obs_data_get_int(d,"background_similarity"));
                    obs_data_set_int(settings,"mask_advanced_key_smoothness",obs_data_get_int(d,"background_smoothness"));
                    obs_data_set_int(settings,"mask_advanced_key_spill_reduction",obs_data_get_int(d,"background_spill"));
                    obs_data_set_double(settings,"mask_advanced_key_opacity",1);obs_data_set_bool(settings,"show_matte",false);
                }else if(i==2){
                    obs_data_set_int(settings,"mask_type",7);obs_data_set_int(settings,"mask_effect",1);
                    obs_data_set_double(settings,"mask_feather_size",obs_data_get_double(d,"background_feather"));
                }else{
                    obs_data_set_int(settings,"mode",mode);obs_data_set_int(settings,"stability",obs_data_get_int(d,"background_stability"));
                    obs_data_set_int(settings,"device",obs_data_get_int(d,"background_device"));
                    obs_data_set_int(settings,"retry",obs_data_get_int(settings,"retry")+1);
                }
                obs_source_update(owned.roles[i],settings);obs_data_release(settings);
            }
            obs_source_set_enabled(owned.roles[i],want[i]);
        }
        // OBS index 0 is the final effect. Key the original camera before colour
        // and shape correction; feather follows the keyer. Other relative order
        // is preserved, and no existing user filter is edited or removed.
        for(int i:{3,1,2})if(want[i]){
            const int effect=obs_source_filter_get_index(parent,owned.roles[i]),face=obs_source_filter_get_index(parent,beauty);
            // Removing an effect ahead of beauty shifts beauty one index down.
            const size_t before=size_t(face+(effect>face?1:0));
            obs_source_filter_set_index(parent,owned.roles[i],std::min(before,obs_source_filter_count(parent)-1));
        }
        obs_property_set_description(obs_properties_get(props,"background_result"),mode==0?"배경 제거를 껐습니다.":mode==1?"자동 배경 제거를 켰습니다.":mode==3?"배경 등록 모드를 켰습니다. 처음 사용하는 경우 빈 배경을 등록해 주세요.":"크로마키를 켰습니다. 지울 배경색과 허용 범위를 조절해 주세요.");
    }else{
        for(int i:created)obs_source_filter_remove(parent,owned.roles[i]);
        obs_property_set_description(obs_properties_get(props,"background_result"),"배경 제거를 시작하지 못했습니다. VEIL을 다시 설치한 뒤 OBS를 재실행해 주세요. 기존 배경 설정은 유지됩니다.");
    }
    for(auto*s:owned.roles)obs_source_release(s);obs_data_release(d);return true;
}
bool changed(obs_properties_t*p,obs_property_t*,obs_data_t*d){background_visibility(p,d);return true;}
std::string video_preview(obs_source_t*beauty,std::string&state){
    auto*parent=beauty?obs_filter_get_parent(beauty):nullptr;Owned owned{beauty?obs_source_get_uuid(beauty):""};
    if(parent)obs_source_enum_filters(parent,collect,&owned);
    std::string html="아직 등록한 배경이 없습니다. 카메라 화면을 비운 뒤 빈 배경을 등록해 주세요.";
    if(owned.roles[3]){auto*p=obs_source_properties(owned.roles[3]);auto*preview=obs_properties_get(p,"reference_preview");if(preview)html=obs_property_description(preview);
        if(obs_source_enabled(owned.roles[3]))if(auto*status=obs_properties_get(p,"status"))state=obs_property_description(status);obs_properties_destroy(p);}
    for(auto*s:owned.roles)obs_source_release(s);return html;
}
bool video_action(obs_properties_t*p,obs_property_t*property,void*opaque){
    auto*beauty=static_cast<obs_source_t*>(opaque);auto*parent=beauty?obs_filter_get_parent(beauty):nullptr;
    Owned owned{beauty?obs_source_get_uuid(beauty):""};if(parent)obs_source_enum_filters(parent,collect,&owned);
    const std::string name=obs_property_name(property);std::string message="배경 제거 방식을 선택한 뒤 '배경 설정 적용'을 먼저 눌러 주세요.";
    if(owned.roles[3]&&obs_source_enabled(owned.roles[3])){
        auto*engine=obs_source_properties(owned.roles[3]);const char*action=name=="background_capture"?"capture":name=="background_forget"?"forget":"refresh";
        obs_property_button_clicked(obs_properties_get(engine,action),owned.roles[3]);
        message=obs_property_description(obs_properties_get(engine,"status"));
        if(auto*preview=obs_properties_get(engine,"reference_preview"))obs_property_set_description(obs_properties_get(p,"background_reference_preview"),obs_property_description(preview));
        obs_properties_destroy(engine);
    }
    obs_property_set_description(obs_properties_get(p,"background_result"),message.c_str());for(auto*s:owned.roles)obs_source_release(s);return true;
}
}
void load_background_engines(){
    load_engine("veil-video-matting","engines/video/bin/64bit/veil-video-matting.dll","engines/video/data",false);
    load_engine("live-backgroundremoval-lite","engines/lite/bin/64bit/live-backgroundremoval-lite.dll","engines/lite/data",true);
    load_engine("obs-advanced-masks","engines/advanced/obs-advanced-masks.dll","engines/advanced/data",false);
    load_engine("veil-background-finish","engines/finish/veil-background-finish.dll","engines/advanced/data",false);
}
void background_defaults(obs_data_t*d){
    obs_data_set_default_int(d,"background_mode",0);obs_data_set_default_int(d,"background_color",0x00ff00);
    obs_data_set_default_int(d,"background_similarity",400);obs_data_set_default_int(d,"background_smoothness",80);
    obs_data_set_default_int(d,"background_spill",100);obs_data_set_default_int(d,"background_stability",75);
    obs_data_set_default_int(d,"background_feather",1);
    obs_data_set_default_int(d,"background_device",0);
}
void background_prepare(obs_source_t*beauty){
    auto*parent=beauty?obs_filter_get_parent(beauty):nullptr;if(!parent)return;
    auto*d=obs_source_get_settings(beauty);Owned owned{obs_source_get_uuid(beauty)};obs_source_enum_filters(parent,collect,&owned);
    if(!obs_data_get_int(d,"background_controls_version")){
        // Adopt the effect values already visible on screen; opening the new UI
        // must not reset a prior adjustment made in a helper's old properties.
        if(owned.roles[3]){auto*s=obs_source_get_settings(owned.roles[3]);
            obs_data_set_int(d,"background_stability",obs_data_get_int(s,"stability"));obs_data_set_int(d,"background_device",obs_data_get_int(s,"device"));
            if(obs_source_enabled(owned.roles[3]))obs_data_set_int(d,"background_mode",obs_data_get_int(s,"mode"));obs_data_release(s);}
        if(owned.roles[2]&&obs_source_enabled(owned.roles[2])){auto*s=obs_source_get_settings(owned.roles[2]);const double size=obs_data_get_double(s,"mask_feather_size");
            if(std::isfinite(size)&&size>=0&&size<=50)obs_data_set_double(d,"background_feather",size);obs_data_release(s);}
        if(owned.roles[1]){auto*s=obs_source_get_settings(owned.roles[1]);
            for(const auto&key:{std::pair{"background_color","advanced_key_color_single"},std::pair{"background_similarity","mask_advanced_key_similarity"},std::pair{"background_smoothness","mask_advanced_key_smoothness"},std::pair{"background_spill","mask_advanced_key_spill_reduction"}})
                obs_data_set_int(d,key.first,obs_data_get_int(s,key.second));
            if(obs_source_enabled(owned.roles[1]))obs_data_set_int(d,"background_mode",2);obs_data_release(s);}
        obs_data_set_int(d,"background_controls_version",1);obs_source_update(beauty,d);
    }
    if(available(finish_id))for(int role:{1,2}){
        auto*old=owned.roles[role];if(!old||std::string(obs_source_get_id(old))!=key_id)continue;
        auto*s=obs_source_get_settings(old);const bool supported=obs_data_get_int(s,"mask_type")== (role==1?6:7)&&obs_data_get_int(s,"mask_effect")==1;obs_data_release(s);if(!supported)continue;
        // Prepare before removing the old effect. Only these stateless finishing
        // roles change type; the person filter UUID and saved photo never move.
        auto*snapshot=obs_save_source(old);obs_data_set_string(snapshot,"id",finish_id);obs_data_set_string(snapshot,"versioned_id",finish_id);obs_data_set_string(snapshot,"uuid","");
        auto*next=obs_load_private_source(snapshot);obs_data_release(snapshot);
        if(next&&obs_obj_get_data(next)){
            const auto index=obs_source_filter_get_index(parent,old);obs_source_filter_remove(parent,old);obs_source_filter_add(parent,next);obs_source_filter_set_index(parent,next,size_t(index));
        }
        obs_source_release(next);
    }
    for(auto*s:owned.roles)obs_source_release(s);obs_data_release(d);
}
void background_visibility(obs_properties_t*p,obs_data_t*d){
    const int mode=int(obs_data_get_int(d,"background_mode"));
    obs_property_set_visible(obs_properties_get(p,"background_ai"),mode==1||mode==3);
    obs_property_set_visible(obs_properties_get(p,"background_key"),mode==2);
    obs_property_set_visible(obs_properties_get(p,"background_feather"),mode!=0);
    obs_property_set_visible(obs_properties_get(p,"background_capture"),mode==3);
    obs_property_set_visible(obs_properties_get(p,"background_forget"),mode==3);
    obs_property_set_visible(obs_properties_get(p,"background_view"),mode==3);
    obs_property_set_visible(obs_properties_get(p,"background_reference_preview"),mode==3);
    obs_property_set_visible(obs_properties_get(p,"background_refresh"),mode==1);
}
void background_properties(obs_properties_t*p,obs_source_t*beauty){
    background_prepare(beauty);
    auto*g=obs_properties_create();auto*mode=obs_properties_add_list(g,"background_mode","배경 제거 방식",OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(mode,"사용 안 함",0);obs_property_list_add_int(mode,"자동 배경 제거",1);obs_property_list_add_int(mode,"배경 등록 후 제거",3);obs_property_list_add_int(mode,"크로마키 · 단색 배경",2);obs_property_set_modified_callback(mode,changed);
    auto*ai=obs_properties_create();obs_properties_add_int_slider(ai,"background_stability","가장자리 떨림 줄이기",0,100,1);
    obs_properties_add_text(ai,"background_ai_help","값을 높이면 움직임이 적은 부분의 잔떨림을 더 줄입니다. 준비 중에는 원본 영상이 보입니다.",OBS_TEXT_INFO);
    auto*device=obs_properties_add_list(ai,"background_device","영상 처리 방식",OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(device,"자동 선택 · 그래픽카드 우선",0);obs_property_list_add_int(device,"프로세서 사용",1);
    obs_properties_add_group(g,"background_ai","가장자리 보정",OBS_GROUP_NORMAL,ai);
    auto*key=obs_properties_create();obs_properties_add_color(key,"background_color","지울 배경색");
    obs_properties_add_int_slider(key,"background_similarity","배경색 허용 범위",1,1000,1);
    obs_properties_add_int_slider(key,"background_smoothness","경계 부드러움",1,1000,1);
    obs_properties_add_int_slider(key,"background_spill","배경색 번짐 줄이기",1,1000,1);
    obs_properties_add_group(g,"background_key","크로마키",OBS_GROUP_NORMAL,key);
    obs_properties_add_float_slider(g,"background_feather","가장자리 부드러움 (픽셀)",0,50,.01);
    obs_properties_add_button2(g,"background_apply","배경 설정 적용",apply,beauty);
    obs_properties_add_button2(g,"background_capture","빈 배경 등록 (3초 후)",video_action,beauty);
    obs_properties_add_button2(g,"background_view","등록한 배경 보기",video_action,beauty);
    obs_properties_add_button2(g,"background_refresh","처리 상태 확인",video_action,beauty);
    obs_properties_add_button2(g,"background_forget","등록한 배경 지우기",video_action,beauty);
    std::string state="방식을 선택한 뒤 '배경 설정 적용'을 눌러 주세요.";const auto html=video_preview(beauty,state);
    obs_properties_add_text(g,"background_result",state.c_str(),OBS_TEXT_INFO);
    obs_properties_add_text(g,"background_reference_preview",html.c_str(),OBS_TEXT_INFO);
    obs_properties_add_text(g,"background_help","자동 배경 제거: 별도 준비 없이 사람을 찾아 배경을 지웁니다.\n배경 등록 후 제거: 같은 인물 인식을 유지하면서, 저장한 사진이 잘 맞는 부분의 윤곽을 다듬습니다. 카메라를 고정해 두고 사용하세요.\n등록 버튼을 누른 뒤 3초 안에 화면에서 나와 1초 동안 비워 주세요. 사진은 이 PC에 저장되며 OBS 종료·VEIL 업데이트 후에도 유지됩니다.\n영상 크기가 다르면 사진을 보관한 채 자동 제거를 사용합니다. 원래 크기로 돌아가면 다시 사용합니다. 카메라 위치·조명이 달라지면 새로 등록해 주세요.\n방송에 넣을 배경 이미지는 OBS에서 카메라 아래에 추가하세요.",OBS_TEXT_INFO);
    obs_properties_add_group(p,"background","배경 제거",OBS_GROUP_NORMAL,g);
}
}
