#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <emmintrin.h>

namespace veil {
// Full-resolution, exact counterparts of the original scalar operations.
// No reciprocal approximation, fast-math, resize or change to temporal weights.
inline void planar_bgra(const uint8_t*input,size_t n,float*output){
    const auto zero=_mm_setzero_ps(),one=_mm_set1_ps(1.f),opaque=_mm_set1_ps(1.f/255.f);
    const auto bytes=_mm_set1_epi32(255),empty=_mm_setzero_si128();size_t i=0;
    for(;i+4<=n;i+=4){
        const auto bgra=_mm_loadu_si128(reinterpret_cast<const __m128i*>(input+i*4));
        const auto alpha=_mm_srli_epi32(bgra,24);
        const auto scale=_mm_movemask_epi8(_mm_cmpeq_epi32(alpha,bytes))==65535?opaque:
            _mm_and_ps(_mm_div_ps(one,_mm_max_ps(one,_mm_cvtepi32_ps(alpha))),
                _mm_castsi128_ps(_mm_cmpgt_epi32(alpha,empty)));
        const auto r=_mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(bgra,16),bytes));
        const auto g=_mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(bgra,8),bytes));
        const auto b=_mm_cvtepi32_ps(_mm_and_si128(bgra,bytes));
        _mm_storeu_ps(output+i,_mm_min_ps(one,_mm_max_ps(zero,_mm_mul_ps(r,scale))));
        _mm_storeu_ps(output+n+i,_mm_min_ps(one,_mm_max_ps(zero,_mm_mul_ps(g,scale))));
        _mm_storeu_ps(output+2*n+i,_mm_min_ps(one,_mm_max_ps(zero,_mm_mul_ps(b,scale))));
    }
    for(;i<n;i++){
        const float scale=input[i*4+3]?1.f/input[i*4+3]:0.f;
        for(int c=0;c<3;c++)output[size_t(c)*n+i]=std::clamp(input[i*4+2-c]*scale,0.f,1.f);
    }
}
inline void stabilize_matte(const uint8_t*source,const uint8_t*previous_source,
    uint8_t*result,const uint8_t*previous_result,size_t n,int stability){
    const float weight=std::clamp(stability,0,100)*.003f;if(!weight)return;
    const auto w=_mm_set1_ps(weight),inverse=_mm_set1_ps(1-weight),half=_mm_set1_ps(.5f);
    const auto zero=_mm_setzero_si128(),bytes=_mm_set1_epi32(255);
    auto blend=[&](__m128i a,__m128i b){return _mm_cvttps_epi32(_mm_add_ps(
        _mm_add_ps(_mm_mul_ps(_mm_cvtepi32_ps(a),inverse),_mm_mul_ps(_mm_cvtepi32_ps(b),w)),half));};
    size_t i=0;
    for(;i+4<=n;i+=4){
        const auto s=_mm_loadu_si128(reinterpret_cast<const __m128i*>(source+i*4));
        const auto p=_mm_loadu_si128(reinterpret_cast<const __m128i*>(previous_source+i*4));
        const auto r=_mm_loadu_si128(reinterpret_cast<const __m128i*>(result+i*4));
        const auto old=_mm_loadu_si128(reinterpret_cast<const __m128i*>(previous_result+i*4));
        const auto motion_bytes=_mm_or_si128(_mm_subs_epu8(s,p),_mm_subs_epu8(p,s));
        const auto motion=_mm_add_epi32(_mm_and_si128(motion_bytes,bytes),_mm_add_epi32(
            _mm_and_si128(_mm_srli_epi32(motion_bytes,8),bytes),_mm_and_si128(_mm_srli_epi32(motion_bytes,16),bytes)));
        const auto delta=_mm_srli_epi32(_mm_or_si128(_mm_subs_epu8(r,old),_mm_subs_epu8(old,r)),24);
        const auto use=_mm_and_si128(_mm_cmpgt_epi32(_mm_set1_epi32(13),motion),_mm_cmpgt_epi32(_mm_set1_epi32(32),delta));
        if(!_mm_movemask_epi8(use))continue;
        const auto a0=_mm_unpacklo_epi8(r,zero),a1=_mm_unpackhi_epi8(r,zero);
        const auto b0=_mm_unpacklo_epi8(old,zero),b1=_mm_unpackhi_epi8(old,zero);
        const auto lo=_mm_packs_epi32(blend(_mm_unpacklo_epi16(a0,zero),_mm_unpacklo_epi16(b0,zero)),
            blend(_mm_unpackhi_epi16(a0,zero),_mm_unpackhi_epi16(b0,zero)));
        const auto hi=_mm_packs_epi32(blend(_mm_unpacklo_epi16(a1,zero),_mm_unpacklo_epi16(b1,zero)),
            blend(_mm_unpackhi_epi16(a1,zero),_mm_unpackhi_epi16(b1,zero)));
        const auto next=_mm_packus_epi16(lo,hi);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(result+i*4),_mm_or_si128(_mm_and_si128(use,next),_mm_andnot_si128(use,r)));
    }
    for(;i<n;i++){
        int motion=0;for(int c=0;c<3;c++)motion+=std::abs(int(source[i*4+c])-int(previous_source[i*4+c]));
        if(motion<=12&&std::abs(int(result[i*4+3])-int(previous_result[i*4+3]))<32)
            for(int c=0;c<4;c++)result[i*4+c]=uint8_t(result[i*4+c]*(1-weight)+previous_result[i*4+c]*weight+.5f);
    }
}
}
