#pragma once
#include <obs-module.h>
namespace veil {
// Thin OBS API adapter; original modules and selected finishing C units remain
// unmodified. Managed helper properties belong to VEIL's integration layer.
void load_background_engines();
void background_defaults(obs_data_t*);
void background_properties(obs_properties_t*,obs_source_t* beauty);
void background_prepare(obs_source_t* beauty);
void background_visibility(obs_properties_t*,obs_data_t*);
}
