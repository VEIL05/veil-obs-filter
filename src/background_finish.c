// GPL-2.0-or-later. Thin VEIL integration of Advanced Masks1.6.1 components.
// C processing units and effect files are unchanged. The full original module
// remains available for unrelated and independently configured Advanced Masks.
#include "managed_background.h"
#include "base-filter.h"
#include "mask-chroma-key.h"
#include "mask-feather.h"
#include "obs-utils.h"
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-advanced-masks","en-US")
struct Finish {base_filter_data_t base;mask_feather_data_t*feather;mask_chroma_key_data_t*key;};

// Same operations as upstream advanced-masks-filter.c:get_input_source.
void get_input_source(base_filter_data_t *filter)
{
    gs_effect_t *pass_through = obs_get_base_effect(OBS_EFFECT_DEFAULT);
    const enum gs_color_space preferred_spaces[] = {GS_CS_SRGB, GS_CS_SRGB_16F, GS_CS_709_EXTENDED};
    const enum gs_color_space source_space = obs_source_get_color_space(obs_filter_get_target(filter->context), OBS_COUNTOF(preferred_spaces), preferred_spaces);
    const enum gs_color_format format = gs_get_format_from_space(source_space);
    filter->input_texrender = create_or_reset_texrender(filter->input_texrender);
    if (obs_source_process_filter_begin_with_color_space(filter->context, format, source_space, OBS_NO_DIRECT_RENDERING) &&
        gs_texrender_begin(filter->input_texrender, filter->width, filter->height)) {
        set_blending_parameters();
        gs_ortho(0.0f, (float)filter->width, 0.0f, (float)filter->height, -100.0f, 100.0f);
        obs_source_process_filter_tech_end(filter->context, pass_through, filter->width, filter->height, "DrawAlphaDivide");
        gs_texrender_end(filter->input_texrender);
        gs_blend_state_pop();
        filter->input_texture_generated = true;
    }
}
static const char*name(void*unused){(void)unused;return "VEIL 배경 보조 처리";}
static void update(void*data,obs_data_t*d){
    struct Finish*f=data;f->base.mask_type=(uint32_t)obs_data_get_int(d,"mask_type");f->base.mask_effect=(uint32_t)obs_data_get_int(d,"mask_effect");
    mask_chroma_key_update(f->key,d);mask_feather_update(f->feather,d);
}
static void*create(obs_data_t*d,obs_source_t*source){
    struct Finish*f=bzalloc(sizeof(*f));f->base.context=source;
    f->key=mask_chroma_key_create();f->feather=mask_feather_create();update(f,d);return f;
}
static void destroy(void*data){
    struct Finish*f=data;mask_chroma_key_destroy(f->key);mask_feather_destroy(f->feather);
    obs_enter_graphics();gs_texrender_destroy(f->base.input_texrender);obs_leave_graphics();bfree(f);
}
static void defaults(obs_data_t*d){obs_data_set_default_int(d,"mask_type",MASK_TYPE_FEATHER);obs_data_set_default_int(d,"mask_effect",MASK_EFFECT_ALPHA);mask_chroma_key_defaults(d);mask_feather_defaults(d);}
static void render(void*data,gs_effect_t*effect){
    (void)effect;struct Finish*f=data;obs_source_t*target=obs_filter_get_target(f->base.context);
    if(!target){obs_source_skip_video_filter(f->base.context);return;}
    f->base.width=obs_source_get_base_width(target);f->base.height=obs_source_get_base_height(target);
    if(f->base.mask_type==MASK_TYPE_CHROMA_KEY)render_chroma_key_mask(f->key,&f->base);
    else if(f->base.mask_type==MASK_TYPE_FEATHER)render_feather_mask(f->feather,&f->base);
    else obs_source_skip_video_filter(f->base.context);
}
static obs_properties_t*properties(void*data){
    struct Finish*f=data;obs_properties_t*p=obs_properties_create();
    if(f&&veil_is_managed(f->base.context)){
        if(f->base.mask_type==MASK_TYPE_CHROMA_KEY){
            obs_properties_add_color(p,"advanced_key_color_single","지울 배경색");
            obs_properties_add_int_slider(p,"mask_advanced_key_similarity","배경색 허용 범위",1,1000,1);
            obs_properties_add_int_slider(p,"mask_advanced_key_smoothness","경계 부드러움",1,1000,1);
            obs_properties_add_int_slider(p,"mask_advanced_key_spill_reduction","배경색 번짐 줄이기",1,1000,1);
            obs_property_set_long_description(obs_properties_get(p,"advanced_key_color_single"),"초록 천 등 지울 단색 배경을 선택합니다. 이 필터를 사용할 때는 ‘VEIL 배경 · 인물’의 눈을 끄세요.");
            obs_property_set_long_description(obs_properties_get(p,"mask_advanced_key_similarity"),"높일수록 선택한 색과 비슷한 색까지 지웁니다.");
            obs_property_set_long_description(obs_properties_get(p,"mask_advanced_key_smoothness"),"선택한 배경과 인물 사이의 경계를 부드럽게 만듭니다.");
            obs_property_set_long_description(obs_properties_get(p,"mask_advanced_key_spill_reduction"),"머리카락과 피부에 비친 배경색을 줄입니다.");
        }else{
            obs_property_t*size=obs_properties_add_float_slider(p,"mask_feather_size","윤곽 부드러움",0,50,.01);
            obs_property_float_set_suffix(size," px");
            obs_property_set_long_description(size,"0은 추가로 흐리지 않습니다. 0~1픽셀부터 조절하세요. 떨림은 ‘VEIL 배경 · 인물’에서 줄일 수 있습니다.");
        }
    }else if(f&&f->base.mask_type==MASK_TYPE_CHROMA_KEY)mask_chroma_key_properties(p);
    else feather_mask_properties(p);
    return p;
}
bool obs_module_load(void){
    struct obs_source_info i={0};i.id="veil_background_finish";i.type=OBS_SOURCE_TYPE_FILTER;
    i.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_SRGB|OBS_SOURCE_CAP_OBSOLETE;
    i.get_name=name;i.create=create;i.destroy=destroy;i.get_defaults=defaults;i.update=update;i.video_render=render;i.get_properties=properties;
    obs_register_source(&i);return true;
}
