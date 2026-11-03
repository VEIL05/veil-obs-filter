#include "beauty_core.h"
#include "landmark_smoother.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace trueitem;
int main(int argc,char**argv){
    assert(argc==2);std::ifstream in(argv[1]);assert(in);int count,cases;in>>count>>cases;assert(count==478);
    LandmarkFrame face;face.valid=true;face.presence=1;for(auto&p:face.points)in>>p.x>>p.y;
    double minimum=10,maximum=0;
    // Maximum controls, conflicting signed controls, every individual control:
    // a displayed grid must keep its orientation (positive Jacobian), with a
    // bounded displacement and untouched pixels outside all influence circles.
    for(int variant=0;variant<21;++variant){
        BeautyParams p;p.slim=p.chin=p.eyes=p.nose=p.lips=0;
        int*keys[]={&p.slim,&p.chin,&p.eyes,&p.nose,&p.lips,&p.faceLength,&p.eyeSpacing,&p.noseLength};
        if(variant<5)for(int k=0;k<8;++k)*keys[k]=((variant&(1<<(k%2)))?-100:100);
        else *keys[(variant-5)%8]=(variant<13?100:-100);
        const auto u=compute_uniforms(face,p,1280,720);assert(u.natural);
        constexpr float eps=.0001f;
        for(int y=0;y<=150;++y)for(int x=0;x<=150;++x){
            Vec2 uv{x/150.f,y/150.f},q=apply_warp(uv,u),a=apply_warp({uv[0]+eps,uv[1]},u),b=apply_warp({uv[0],uv[1]+eps},u);
            const double det=((a[0]-q[0])*(b[1]-q[1])-(a[1]-q[1])*(b[0]-q[0]))/(eps*eps);
            minimum=std::min(minimum,det);maximum=std::max(maximum,double(std::hypot((q[0]-uv[0])*u.aspect,q[1]-uv[1])));
            assert(std::isfinite(det)&&det>.45&&det<1.65);assert(std::hypot(q[0]-uv[0],q[1]-uv[1])<.035);
        }
        const auto outside=apply_warp({0,0},u);assert(outside[0]==0&&outside[1]==0);
    }
    LandmarkSmoother smoother;double raw_energy=0,filtered_energy=0,moving_error=0;int moving_count=0;
    for(int n=1;n<240;++n){
        LandmarkFrame f;f.valid=true;f.presence=1;f.timestamp_ns=std::uint64_t(n)*33333333;
        const float truth=n<120?.5f:.5f+float(n-120)*.008f;
        const float noise=(n%2?.0015f:-.0015f);
        for(auto&p:f.points){p.x=truth+noise;p.y=.5f+noise;}
        smoother.filter(f);
        if(n>30&&n<120){raw_energy+=noise*noise;filtered_energy+=std::pow(f.points[0].x-truth,2);}
        if(n>160){moving_error+=std::abs(f.points[0].x-truth);++moving_count;}
    }
    assert(filtered_energy<raw_energy*.15);assert(moving_error/moving_count<.012);
    smoother.reset();LandmarkFrame fresh;fresh.timestamp_ns=9000000000;fresh.points[0].x=.25f;smoother.filter(fresh);assert(fresh.points[0].x==.25f);
    std::cout<<"PASS natural shape grid 21 combinations: min Jacobian "<<minimum<<", max displacement "<<maximum
             <<"; static jitter energy ratio "<<filtered_energy/raw_energy<<", motion lag "<<moving_error/moving_count<<" normalized\n";
}
