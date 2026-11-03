// Conservative registered-background cleanup after recurrent human matting.
#pragma once
#include "matting_model.h"
namespace veil {
// A validated photo can refine uncertain outer edges by at most 8/255; opaque
// body pixels remain untouched. Incompatible photos leave the automatic result.
bool refine_registered_background(const MatteFrame&source,const MatteFrame&plate,MatteFrame&result);
// Keep automatic temporal history BEFORE photo correction, so photo decisions
// cannot feed back into subsequent human masks. A null plate is automatic mode.
bool postprocess_matte(const MatteFrame&source,const MatteFrame*plate,MatteFrame&result,
    const MatteFrame*previous_source,const MatteFrame*previous_automatic,int stability,
    MatteFrame*automatic_history);
}
