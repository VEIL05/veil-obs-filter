#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <emmintrin.h>

namespace veil {
// Exact full-resolution BGRA composition. SSE2 is available on Windows x64;
// no downscaling, relaxed floating-point mode or change to alpha semantics.
inline void compose_matte(const uint8_t*source,const float*alpha,const float*rgb,size_t n,uint8_t*output){
    const auto zero=_mm_setzero_ps(),one=_mm_set1_ps(1.f),scale=_mm_set1_ps(255.f),half=_mm_set1_ps(.5f);
    const auto inverse=_mm_set1_ps(1.f/255.f),sign=_mm_set1_ps(-0.f),finite=_mm_set1_ps(std::numeric_limits<float>::max());
    auto quantize=[&](__m128 v){return _mm_cvttps_epi32(_mm_add_ps(_mm_mul_ps(v,scale),half));};
    size_t i=0;
    for(;i+4<=n;i+=4){
        auto a=_mm_loadu_ps(alpha+i),r=_mm_loadu_ps(rgb+i),g=_mm_loadu_ps(rgb+n+i),b=_mm_loadu_ps(rgb+2*n+i);
        auto valid=_mm_and_ps(_mm_cmple_ps(_mm_andnot_ps(sign,a),finite),_mm_cmple_ps(_mm_andnot_ps(sign,r),finite));
        valid=_mm_and_ps(valid,_mm_and_ps(_mm_cmple_ps(_mm_andnot_ps(sign,g),finite),_mm_cmple_ps(_mm_andnot_ps(sign,b),finite)));
        if(_mm_movemask_ps(valid)!=15)throw std::runtime_error("Non-finite matting output");
        const auto pixel=_mm_loadu_si128(reinterpret_cast<const __m128i*>(source+i*4));
        const auto input_alpha=_mm_mul_ps(_mm_cvtepi32_ps(_mm_srli_epi32(pixel,24)),inverse);
        const auto coverage=_mm_mul_ps(_mm_min_ps(_mm_max_ps(a,zero),one),input_alpha);
        auto byte_b=quantize(_mm_mul_ps(_mm_min_ps(_mm_max_ps(b,zero),one),coverage));
        auto byte_g=quantize(_mm_mul_ps(_mm_min_ps(_mm_max_ps(g,zero),one),coverage));
        auto byte_r=quantize(_mm_mul_ps(_mm_min_ps(_mm_max_ps(r,zero),one),coverage));
        auto packed=_mm_or_si128(byte_b,_mm_slli_epi32(byte_g,8));
        packed=_mm_or_si128(packed,_mm_or_si128(_mm_slli_epi32(byte_r,16),_mm_slli_epi32(quantize(coverage),24)));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(output+i*4),packed);
    }
    auto byte=[](float v){return uint8_t(std::clamp(v,0.f,1.f)*255.f+.5f);};
    for(;i<n;i++){
        if(!std::isfinite(alpha[i])||!std::isfinite(rgb[i])||!std::isfinite(rgb[n+i])||!std::isfinite(rgb[2*n+i]))throw std::runtime_error("Non-finite matting output");
        const auto coverage=std::clamp(alpha[i],0.f,1.f)*(source[i*4+3]/255.f);
        for(int c=0;c<3;c++)output[i*4+c]=byte(std::clamp(rgb[(2-c)*n+i],0.f,1.f)*coverage);
        output[i*4+3]=byte(coverage);
    }
}
}
