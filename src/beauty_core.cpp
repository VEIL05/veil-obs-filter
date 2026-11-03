// Direct math/mask port of ctbot000/face-beautifier ca5ad40 (MIT, CTBot000 2026).
// Region rasterisation uses deterministic 4-subscanline coverage and a separable
// Gaussian in place of browser Canvas2D fill/filter; see tests/math-contract.md.
#include "beauty_core.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace trueitem {
namespace {
constexpr int oval[] = {10,338,297,332,284,251,389,356,454,323,361,288,397,365,379,378,400,377,152,148,176,149,150,136,172,58,132,93,234,127,162,21,54,103,67,109};
constexpr int lips_outer[] = {61,146,91,181,84,17,314,405,321,375,291,409,270,269,267,0,37,39,40,185};
constexpr int lips_inner[] = {78,95,88,178,87,14,317,402,318,324,308,415,310,311,312,13,82,81,80,191};
constexpr int eye_l[] = {263,249,390,373,374,380,381,382,362,398,384,385,386,387,388,466};
constexpr int eye_r[] = {33,7,163,144,145,153,154,155,133,173,157,158,159,160,161,246};
constexpr int brow_l[] = {276,283,282,295,285,336,296,334,293,300};
constexpr int brow_r[] = {46,53,52,65,55,107,66,105,63,70};
constexpr int jaw_l[] = {356,454,323,361,288,397,365,379,378,400,377};
constexpr int jaw_r[] = {127,234,93,132,58,172,136,150,149,176,148};
constexpr int jaw_pick[] = {1,3,4,6,7};
constexpr float jaw_profile[] = {.45f,.85f,1.f,.95f,.72f};
float length(Vec2 p) { return std::hypot(p[0],p[1]); }
Vec2 sub(Vec2 a, Vec2 b) { return {a[0]-b[0],a[1]-b[1]}; }
Vec2 add(Vec2 a, Vec2 b) { return {a[0]+b[0],a[1]+b[1]}; }
Vec2 mul(Vec2 a,float s) { return {a[0]*s,a[1]*s}; }
Vec2 mid(Vec2 a, Vec2 b) { return mul(add(a,b),.5f); }
Vec2 norm(Vec2 a) { float n=length(a);return mul(a,n>0?1.f/n:1.f); }
float distance(Vec2 a,Vec2 b) { return length(sub(a,b)); }
bool usable(const LandmarkFrame &l) {
    if (!l.valid || !(l.presence>0)) return false;
    for (const auto &p:l.points) if (!std::isfinite(p.x)||!std::isfinite(p.y)) return false;
    return true;
}
struct Face {
    bool valid=false;
    float width{},height{},nose_width{},lip_width{},eye_width_l{},eye_width_r{};
    Vec2 chin{},brow{},axis{},eye_l{},eye_r{},cheek_l{},cheek_r{},nose_l{},nose_r{},nose_center{},lip_center{};
};
Face compute_face(const LandmarkFrame &l,float aspect) {
    Face f;
    if(!usable(l)) return f;
    auto pt=[&](int i)->Vec2{return {l.points[i].x*aspect,l.points[i].y};};
    f.chin=pt(152);f.brow=pt(10);f.width=distance(pt(454),pt(234));f.height=distance(f.chin,f.brow);
    if(!(f.width>1e-4f)||!(f.height>1e-4f)) return f;
    f.valid=true;f.axis=norm(sub(f.chin,f.brow));
    f.eye_l=pt(473);f.eye_r=pt(468);f.eye_width_l=distance(pt(263),pt(362));f.eye_width_r=distance(pt(33),pt(133));
    f.cheek_l=mid(pt(263),pt(291));f.cheek_r=mid(pt(33),pt(61));
    f.nose_l=pt(278);f.nose_r=pt(48);f.nose_center=mid(f.nose_l,f.nose_r);f.nose_width=distance(f.nose_l,f.nose_r);
    f.lip_center=mid(mid(pt(291),pt(61)),mid(pt(13),pt(14)));f.lip_width=distance(pt(291),pt(61));return f;
}
void push_move(CoreUniforms &u,Vec2 from,Vec2 delta,float radius) {
    float len=length(delta);
    if(!(radius>1e-5f)||len<1e-6f||u.warp_count>=max_warp) return;
    float cap=radius*.45f;if(len>cap)delta=mul(delta,cap/len);
    int n=u.warp_count++;u.warp_p[n]={from[0]+delta[0],from[1]+delta[1],radius,0};u.warp_q[n]={delta[0],delta[1],0,0};
}
void push_scale(CoreUniforms &u,Vec2 center,float radius,float scale) {
    if(!(radius>1e-5f)||std::abs(scale)<1e-5f||u.warp_count>=max_warp)return;
    int n=u.warp_count++;u.warp_p[n]={center[0],center[1],radius,1};u.warp_q[n]={0,0,scale,0};
}
std::vector<float> gaussian(const std::vector<float> &src,int w,int h,float sigma) {
    int radius=static_cast<int>(std::ceil(sigma*3));std::vector<float> kernel(2*radius+1);
    float sum=0;for(int i=-radius;i<=radius;++i){float v=std::exp(-float(i*i)/(2*sigma*sigma));kernel[i+radius]=v;sum+=v;}
    for(float &v:kernel)v/=sum;
    std::vector<float> tmp(src.size(),0),out(src.size(),0);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){float v=0;for(int k=-radius;k<=radius;++k)if(x+k>=0&&x+k<w)v+=src[y*w+x+k]*kernel[k+radius];tmp[y*w+x]=v;}
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){float v=0;for(int k=-radius;k<=radius;++k)if(y+k>=0&&y+k<h)v+=tmp[(y+k)*w+x]*kernel[k+radius];out[y*w+x]=v;}
    return out;
}
template<std::size_t N> std::vector<float> region(const LandmarkFrame &l,const int (&idx)[N],float scale,int w,int h,float sigma) {
    Vec2 center{};for(int i:idx){center[0]+=l.points[i].x;center[1]+=l.points[i].y;}center=mul(center,1.f/N);
    std::array<Vec2,N> polygon;for(std::size_t i=0;i<N;++i)polygon[i]={(center[0]+(l.points[idx[i]].x-center[0])*scale)*w,(center[1]+(l.points[idx[i]].y-center[1])*scale)*h};
    std::vector<float> alpha(w*h,0);std::vector<float> cross;cross.reserve(N);
    for(int y=0;y<h;++y)for(int sample=0;sample<4;++sample){
        float scan=y+(sample+.5f)/4;cross.clear();
        for(std::size_t i=0,j=N-1;i<N;j=i++){
            auto a=polygon[j],b=polygon[i];if((a[1]<=scan&&b[1]>scan)||(b[1]<=scan&&a[1]>scan))cross.push_back(a[0]+(scan-a[1])*(b[0]-a[0])/(b[1]-a[1]));
        }
        std::sort(cross.begin(),cross.end());
        for(std::size_t i=0;i+1<cross.size();i+=2){float left=std::max(0.f,cross[i]),right=std::min(float(w),cross[i+1]);
            if(right<=left)continue;
            int start=std::max(0,int(std::floor(left))),end=std::min(w,int(std::ceil(right)));
            for(int x=start;x<end;++x)alpha[y*w+x]+=.25f*std::max(0.f,std::min(right,float(x+1))-std::max(left,float(x)));
        }
    }
    return gaussian(alpha,w,h,sigma);
}
} // namespace

ShaderParams to_shader(const BeautyParams &v) {
    ShaderParams p;
    p.smooth=v.smooth/100.f;p.texture=v.texture/100.f;p.even=v.even/100.f;p.sharpen=v.sharpen*.006f;p.radius=.5f+v.radius*.015f;
    p.slim=v.slim/100.f;p.chin=v.chin/100.f;p.eyes=v.eyes/100.f;p.nose=v.nose/100.f;p.lips=v.lips/100.f;
    p.eyeBright=v.eyeBright/100.f;p.underEye=v.underEye/100.f;p.lipAmount=v.lipAmount*.0092f;p.blushAmount=v.blushAmount*.0085f;
    p.lipColor=v.lip_rgb;p.blushColor=v.blush_rgb;p.bright=v.bright*.0045f;p.warmth=v.warmth/100.f;p.vibrance=v.vibrance*.006f;
    p.contrast=v.contrast*.004f;p.glow=v.glow/100.f;p.vignette=v.vignette*.006f;p.grain=v.grain/100.f;
    p.skinBright=v.skinBright/100.f;p.redness=v.redness/100.f;p.teethWhite=v.teethWhite/100.f;p.lipGloss=v.lipGloss/100.f;
    p.faceLength=v.faceLength/100.f;p.eyeSpacing=v.eyeSpacing/100.f;p.noseLength=v.noseLength/100.f;return p;
}

CoreUniforms compute_uniforms(const LandmarkFrame &landmarks,const BeautyParams &params,std::uint32_t width,std::uint32_t height) {
    CoreUniforms u;u.shader=to_shader(params);u.aspect=height?float(width)/height:1;u.mirror=params.mirror;u.natural=params.natural;
    Face f=compute_face(landmarks,u.aspect);auto &p=u.shader;
    // Scale coordinates, rather than cross-fading two differently shaped faces.
    const float strength=std::clamp(params.strength/100.f,0.f,1.f)*(params.natural?std::clamp(landmarks.presence,0.f,1.f):1.f);
    p.slim*=strength;p.chin*=strength;p.eyes*=strength;p.nose*=strength;p.lips*=strength;
    p.faceLength*=strength;p.eyeSpacing*=strength;p.noseLength*=strength;
    u.blur_scale=std::clamp(f.valid?f.width*height*.0125f:height*.0034f,1.1f,10.f)*p.radius;
    if(!f.valid)return u;
    u.face=std::clamp(landmarks.presence,0.f,1.f);
    auto pt=[&](int i)->Vec2{return {landmarks.points[i].x*u.aspect,landmarks.points[i].y};};
    if(p.slim>.001f)for(const int *jaw:{jaw_l,jaw_r})for(int i=0;i<5;++i){Vec2 q=pt(jaw[jaw_pick[i]]),v=sub(q,f.brow);float t=v[0]*f.axis[0]+v[1]*f.axis[1];Vec2 dir=sub(add(f.brow,mul(f.axis,t)),q);push_move(u,q,mul(norm(dir),std::min(length(dir)*.9f,f.width*.16f*p.slim*jaw_profile[i])),f.width*.30f);}
    if(std::abs(p.chin)>.001f)push_move(u,f.chin,mul(f.axis,-p.chin*f.height*.085f),f.width*.34f);
    if(p.eyes>.001f){push_scale(u,f.eye_l,std::max(f.eye_width_l*1.35f,f.width*.10f),p.eyes*.20f);push_scale(u,f.eye_r,std::max(f.eye_width_r*1.35f,f.width*.10f),p.eyes*.20f);}
    if(p.nose>.001f)for(auto wing:{f.nose_l,f.nose_r})push_move(u,wing,mul(sub(f.nose_center,wing),p.nose*.30f),std::max(f.nose_width,f.width*.07f));
    if(std::abs(p.lips)>.001f)push_scale(u,f.lip_center,std::max(f.lip_width*.85f,f.width*.12f),p.lips*.13f);
    if(std::abs(p.faceLength)>.001f){
        push_move(u,f.brow,mul(f.axis,-p.faceLength*f.height*.04f),f.width*.34f);
        push_move(u,f.chin,mul(f.axis,p.faceLength*f.height*.04f),f.width*.34f);
    }
    if(std::abs(p.eyeSpacing)>.001f){
        Vec2 delta=mul(norm(sub(f.eye_l,f.eye_r)),p.eyeSpacing*f.width*.035f);
        push_move(u,f.eye_l,delta,f.width*.18f);push_move(u,f.eye_r,mul(delta,-1),f.width*.18f);
    }
    if(std::abs(p.noseLength)>.001f)push_move(u,pt(1),mul(f.axis,p.noseLength*f.height*.04f),f.width*.18f);
    if(params.natural)for(int i=0;i<u.warp_count;++i){
        auto&h=u.warp_p[i];auto&q=u.warp_q[i];
        if(h[3]<.5f){
            const float size=std::hypot(q[0],q[1]),cap=h[2]*.16f;
            if(size>cap){const float k=cap/size;h[0]-=q[0]*(1-k);h[1]-=q[1]*(1-k);q[0]*=k;q[1]*=k;}
        } else q[2]=std::clamp(q[2],-.10f,.10f);
    }
    u.mouth={f.lip_center[0],f.lip_center[1],std::max(f.lip_width*.48f,.001f),1};
    float l=std::max(f.eye_width_l,1e-4f)*.62f,r=std::max(f.eye_width_r,1e-4f)*.62f,c=f.width*.19f;
    u.eye_l={f.eye_l[0],f.eye_l[1],l,1};u.eye_r={f.eye_r[0],f.eye_r[1],r,1};u.cheek_l={f.cheek_l[0],f.cheek_l[1],c,1};u.cheek_r={f.cheek_r[0],f.cheek_r[1],c,1};return u;
}

Vec2 apply_warp(Vec2 uv,const CoreUniforms &u) {
    Vec2 p{uv[0]*u.aspect,uv[1]};
    if(u.natural){
        Vec2 shift{};float weight=0;
        for(int i=0;i<u.warp_count&&i<max_warp;++i){
            const auto h=u.warp_p[i],q=u.warp_q[i];const Vec2 d=sub(p,{h[0],h[1]});const float t=length(d)/h[2];
            if(t>=1)continue;const float fall=1-t*t*(3-2*t);
            shift=add(shift,mul(h[3]<.5f?Vec2{q[0],q[1]}:mul(d,q[2]),fall));weight+=fall;
        }
        p=sub(p,mul(shift,1.f/(1.f+weight)));
        return {p[0]/u.aspect,p[1]};
    }
    for(int i=0;i<u.warp_count&&i<max_warp;++i){auto h=u.warp_p[i],q=u.warp_q[i];Vec2 d{sub(p,{h[0],h[1]})};float dd=d[0]*d[0]+d[1]*d[1];if(dd>=h[2]*h[2])continue;
        float t=std::sqrt(dd)/h[2],fall=1-t*t*(3-2*t);if(h[3]<.5f)p=sub(p,mul({q[0],q[1]},fall));else p=add({h[0],h[1]},mul(d,1-q[2]*fall));}
    return {p[0]/u.aspect,p[1]};
}

MaskImage build_mask(const LandmarkFrame &landmarks,float aspect,int width) {
    MaskImage out;out.width=std::clamp(width,32,1024);out.height=std::max(32,int(std::round(out.width/std::max(.2f,aspect))));
    out.rgba.assign(std::size_t(out.width)*out.height*4,0);for(std::size_t i=3;i<out.rgba.size();i+=4)out.rgba[i]=255;
    if(!usable(landmarks))return out;
    int w=out.width,h=out.height;float soft=std::max(2.f,w*.024f);
    auto red=region(landmarks,oval,.965f,w,h,soft*1.35f),green=region(landmarks,lips_outer,1.f,w,h,soft*.6f);
    auto blue=region(landmarks,eye_l,1.08f,w,h,soft*.6f);
    auto add_blue=[&](const auto &indices,float scale){auto mask=region(landmarks,indices,scale,w,h,soft*.6f);for(std::size_t i=0;i<blue.size();++i)blue[i]=std::min(1.f,blue[i]+mask[i]);};
    add_blue(eye_r,1.08f);add_blue(lips_inner,.9f);
    auto exclude_red=[&](const auto &indices,float scale){auto mask=region(landmarks,indices,scale,w,h,soft*.8f);for(std::size_t i=0;i<red.size();++i)red[i]*=1-mask[i];};
    exclude_red(lips_outer,1.18f);exclude_red(eye_l,1.45f);exclude_red(eye_r,1.45f);exclude_red(brow_l,1.3f);exclude_red(brow_r,1.3f);
    for(std::size_t i=0;i<red.size();++i){out.rgba[i*4]=std::uint8_t(std::lround(std::clamp(red[i],0.f,1.f)*255));out.rgba[i*4+1]=std::uint8_t(std::lround(std::clamp(green[i],0.f,1.f)*255));out.rgba[i*4+2]=std::uint8_t(std::lround(std::clamp(blue[i],0.f,1.f)*255));}
    return out;
}
} // namespace trueitem
