#pragma once
#include <obs-module.h>
#include <string.h>
// A copied/orphan filter stays editable. Names never establish ownership.
struct veil_owner_query {const char*uuid;bool found;};
static inline void veil_find_owner(obs_source_t*parent,obs_source_t*child,void*opaque){
    (void)parent;struct veil_owner_query*q=(struct veil_owner_query*)opaque;
    if(!strcmp(obs_source_get_id(child),"trueitem_beauty_filter")&&!strcmp(obs_source_get_uuid(child),q->uuid))q->found=true;
}
static inline bool veil_is_managed(obs_source_t*source){
    obs_source_t*parent=source?obs_filter_get_parent(source):NULL;if(!parent)return false;
    obs_data_t*d=obs_source_get_settings(source);const int role=(int)obs_data_get_int(d,"veil_background_role");
    struct veil_owner_query q={obs_data_get_string(d,"veil_background_owner"),false};
    if(*q.uuid&&role>=1&&role<=3)obs_source_enum_filters(parent,veil_find_owner,&q);
    obs_data_release(d);return q.found;
}
