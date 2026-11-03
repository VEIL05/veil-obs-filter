#pragma once
#include "matting_model.h"
namespace veil {
// A thumbnail of an explicitly registered reference, held only in memory.
std::string reference_preview_html(const MatteFrame&frame);
}
