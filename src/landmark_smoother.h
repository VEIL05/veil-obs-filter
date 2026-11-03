// Speed-adaptive 1 Euro filtering (Casiez et al., CHI 2012).
// Normalized coordinates: beta must be calibrated in these units.
#pragma once
#include "beauty_types.h"
#include <algorithm>
#include <cmath>

namespace trueitem {
class LandmarkSmoother {
    std::array<float,landmark_count*2> value_{}, raw_{}, velocity_{};
    std::uint64_t last_=0;
public:
    void reset() { last_=0; }
    void filter(LandmarkFrame& frame) {
        const double dt=last_?double(frame.timestamp_ns-last_)/1e9:0;
        if(!last_ || !(dt>0) || dt>.2) {
            for(int i=0;i<landmark_count;++i) {
                value_[i*2]=raw_[i*2]=frame.points[i].x;
                value_[i*2+1]=raw_[i*2+1]=frame.points[i].y;
            }
            velocity_.fill(0);last_=frame.timestamp_ns;return;
        }
        last_=frame.timestamp_ns;
        auto alpha=[dt](double cutoff){return float(1/(1+1/(6.283185307179586*cutoff*dt)));};
        const float derivative_alpha=alpha(1.0);
        for(int i=0;i<landmark_count;++i)for(int j=0;j<2;++j) {
            const int k=i*2+j;const float input=j?frame.points[i].y:frame.points[i].x;
            velocity_[k]+=derivative_alpha*((input-raw_[k])/float(dt)-velocity_[k]);
            raw_[k]=input;
            value_[k]+=alpha(.8+40*std::abs(velocity_[k]))*(input-value_[k]);
            if(j)frame.points[i].y=value_[k];else frame.points[i].x=value_[k];
        }
    }
};
}
