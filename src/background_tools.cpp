// Copyright (C) 2026 TRUE ITEM contributors. GPL-2.0-or-later.
// API-only integration of unmodified GPL upstream OBS filter modules.
#include "background_tools.h"
#include "registered_background_feature.h"
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
bool apply_preset(obs_source_t*beauty,obs_data_t*d){
    if(!beauty)return false;
    auto*parent=obs_filter_get_parent(beauty);
    if(!parent)return false;
    const int requested=std::clamp(int(obs_data_get_int(d,"background_mode")),0,3);
    const int mode=requested==3&&!registered_background_enabled?1:requested;
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
                    // Device is local to this processing filter, not a preset.
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
    }else{
        for(int i:created)obs_source_filter_remove(parent,owned.roles[i]);
    }
    for(auto*s:owned.roles)obs_source_release(s);return success;
}
bool setup(obs_properties_t*p,obs_property_t*,void*opaque){
    auto*beauty=static_cast<obs_source_t*>(opaque);auto*parent=beauty?obs_filter_get_parent(beauty):nullptr;
    if(!parent){auto*result=obs_properties_get(p,"background_result");obs_property_set_description(result,"카메라 소스에 VEIL을 추가해 주세요.");obs_property_set_visible(result,true);obs_property_text_set_info_type(result,OBS_TEXT_INFO_ERROR);return true;}
    Owned owned{obs_source_get_uuid(beauty)};obs_source_enum_filters(parent,collect,&owned);
    bool success=true;std::vector<obs_source_t*>created;
    // Creation is idempotent. Existing settings, enable states and ordering are
    // never reapplied from the beauty filter's legacy preset fields.
    const bool has_active_key=owned.roles[1]&&obs_source_enabled(owned.roles[1]);
    for(int role:{3,2,1})if(!owned.roles[role]){
        const char*id=role==3?video_id:finish_id;
        if(!available(id)){success=false;break;}
        auto*d=obs_get_source_defaults(id);obs_data_set_string(d,"veil_background_owner",owned.owner.c_str());obs_data_set_int(d,"veil_background_role",role);
        if(role!=3){obs_data_set_int(d,"mask_type",role==1?6:7);obs_data_set_int(d,"mask_effect",1);
            if(role==2)obs_data_set_double(d,"mask_feather_size",1);
            else{obs_data_set_int(d,"key_type",1);obs_data_set_int(d,"advanced_color_type",1);obs_data_set_int(d,"advanced_key_color_single",0x00ff00);
                obs_data_set_int(d,"mask_advanced_key_similarity",400);obs_data_set_int(d,"mask_advanced_key_smoothness",80);obs_data_set_int(d,"mask_advanced_key_spill_reduction",100);}}
        auto*s=obs_source_create_private(id,role_names[role],d);obs_data_release(d);
        if(!s||!obs_obj_get_data(s)){obs_source_release(s);success=false;break;}
        obs_source_set_enabled(s,role!=1&&!has_active_key);obs_source_filter_add(parent,s);created.push_back(s);owned.roles[role]=s;
        const int index=obs_source_filter_get_index(parent,beauty);
        obs_source_filter_set_index(parent,s,std::min(size_t(index+1),obs_source_filter_count(parent)-1));
    }
    if(!success)for(auto*s:created)obs_source_filter_remove(parent,s);
    auto*result=obs_properties_get(p,"background_result");
    obs_property_set_description(result,success?"준비되었습니다. 왼쪽에서 배경 필터를 선택하세요.":"추가하지 못했습니다. VEIL을 다시 설치해 주세요.");
    obs_property_set_visible(result,true);obs_property_text_set_info_type(result,success?OBS_TEXT_INFO_NORMAL:OBS_TEXT_INFO_ERROR);
    for(auto*s:owned.roles)obs_source_release(s);return true;
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
    Owned owned{obs_source_get_uuid(beauty)};obs_source_enum_filters(parent,collect,&owned);
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
    for(auto*s:owned.roles)obs_source_release(s);
}
void background_snapshot(obs_source_t*beauty,obs_data_t*target){
    if(!registered_background_enabled&&obs_data_get_int(target,"background_mode")==3)obs_data_set_int(target,"background_mode",1);
    auto*parent=beauty?obs_filter_get_parent(beauty):nullptr;if(!parent)return;
    Owned owned{obs_source_get_uuid(beauty)};obs_source_enum_filters(parent,collect,&owned);
    if(owned.roles[1]||owned.roles[2]||owned.roles[3]){
        obs_data_set_int(target,"background_mode",0);
        if(owned.roles[3]){auto*d=obs_source_get_settings(owned.roles[3]);
            obs_data_set_int(target,"background_stability",obs_data_get_int(d,"stability"));obs_data_set_int(target,"background_device",obs_data_get_int(d,"device"));
            if(obs_source_enabled(owned.roles[3]))obs_data_set_int(target,"background_mode",registered_background_enabled&&obs_data_get_int(d,"mode")==3?3:1);obs_data_release(d);}
        obs_data_set_double(target,"background_feather",0);
        if(owned.roles[2]&&obs_source_enabled(owned.roles[2])){auto*d=obs_source_get_settings(owned.roles[2]);obs_data_set_double(target,"background_feather",obs_data_get_double(d,"mask_feather_size"));obs_data_release(d);}
        if(owned.roles[1]){auto*d=obs_source_get_settings(owned.roles[1]);
            for(const auto&key:{std::pair{"background_color","advanced_key_color_single"},std::pair{"background_similarity","mask_advanced_key_similarity"},std::pair{"background_smoothness","mask_advanced_key_smoothness"},std::pair{"background_spill","mask_advanced_key_spill_reduction"}})
                obs_data_set_int(target,key.first,obs_data_get_int(d,key.second));
            if(obs_source_enabled(owned.roles[1]))obs_data_set_int(target,"background_mode",2);obs_data_release(d);}
    }
    for(auto*s:owned.roles)obs_source_release(s);
}
bool background_apply_preset(obs_source_t*beauty,obs_data_t*settings){return apply_preset(beauty,settings);}
void background_properties(obs_properties_t*p,obs_source_t*beauty){
    background_prepare(beauty);auto*g=obs_properties_create();
    obs_properties_add_text(g,"background_help","인물 제거 → VEIL 배경 · 인물\n윤곽 보정 → VEIL 배경 · 가장자리\n단색 배경 → VEIL 배경 · 크로마키",OBS_TEXT_INFO);
    obs_property_set_long_description(obs_properties_add_button2(g,"background_setup","배경 필터 추가",setup,beauty),"없는 필터만 추가합니다. 기존 값과 켜짐 상태는 유지합니다. 인물 제거와 크로마키는 사용할 필터 하나만 켜세요.");
    auto*result=obs_properties_add_text(g,"background_result","",OBS_TEXT_INFO);obs_property_set_visible(result,false);
    obs_properties_add_group(p,"background","배경 필터 안내",OBS_GROUP_NORMAL,g);
}
}
