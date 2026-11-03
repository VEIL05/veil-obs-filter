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
        obs_properties_add_text(p,"managed_help","이 효과는 VEIL 필터에서 관리합니다.\n조절하려면 ‘VEIL 필터 → 배경 제거’를 여세요.\n이 목록에서 눈을 끄면 해당 효과가 꺼집니다.",OBS_TEXT_INFO);
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
