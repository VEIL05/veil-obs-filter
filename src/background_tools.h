#pragma once
#include <obs-module.h>
namespace veil {
// Thin OBS API adapter; original modules and selected finishing C units remain
// unmodified. Managed helper properties belong to VEIL's integration layer.
void load_background_engines();
void background_defaults(obs_data_t*);
void background_properties(obs_properties_t*,obs_source_t* beauty);
void background_prepare(obs_source_t* beauty);
// Portable presets snapshot the filters that actually render; ordinary edits
// stay with each processing filter and never fan out from stale beauty fields.
void background_snapshot(obs_source_t* beauty,obs_data_t* target);
bool background_apply_preset(obs_source_t* beauty,obs_data_t* settings);
}
