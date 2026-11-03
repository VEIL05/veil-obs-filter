#include "matting_pixels.h"
#include <iostream>
#include <random>
#include <vector>
int main(){try{
    std::mt19937 random(42);std::uniform_real_distribution<float>value(-.25f,1.25f);
    size_t checked=0;int maximum=0;
    for(size_t n:{1,3,4,7,16,4099}){
        std::vector<uint8_t>source(n*4),actual(n*4),expected(n*4);
        std::vector<float>alpha(n),rgb(n*3);
        for(auto&v:source)v=uint8_t(random());for(auto&v:alpha)v=value(random);for(auto&v:rgb)v=value(random);
        for(size_t i=0;i<n;i++){
            const auto coverage=std::clamp(alpha[i],0.f,1.f)*(source[i*4+3]/255.f);
            auto byte=[](float v){return uint8_t(std::clamp(v,0.f,1.f)*255.f+.5f);};
            for(int c=0;c<3;c++)expected[i*4+c]=byte(std::clamp(rgb[(2-c)*n+i],0.f,1.f)*coverage);
            expected[i*4+3]=byte(coverage);
        }
        veil::compose_matte(source.data(),alpha.data(),rgb.data(),n,actual.data());
        for(size_t i=0;i<actual.size();i++){int delta=std::abs(int(actual[i])-int(expected[i]));maximum=std::max(maximum,delta);if(delta>1)throw std::runtime_error("Composition differs from scalar contract");}
        for(size_t i=0;i<n;i++)for(int c=0;c<3;c++)if(actual[i*4+c]>actual[i*4+3])throw std::runtime_error("Premultiplied alpha violated");
        for(size_t index:{size_t(0),n-1})for(float bad:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
            auto valid=alpha[index];alpha[index]=bad;bool rejected=false;
            try{veil::compose_matte(source.data(),alpha.data(),rgb.data(),n,actual.data());}catch(const std::runtime_error&){rejected=true;}
            if(!rejected)throw std::runtime_error("Non-finite alpha accepted");alpha[index]=valid;
            valid=rgb[n+index];rgb[n+index]=bad;rejected=false;
            try{veil::compose_matte(source.data(),alpha.data(),rgb.data(),n,actual.data());}catch(const std::runtime_error&){rejected=true;}
            if(!rejected)throw std::runtime_error("Non-finite RGB accepted");rgb[n+index]=valid;
        }
        checked+=n;
    }
    std::cout<<"PASS "<<checked<<" pixels, scalar maximum byte difference "<<maximum<<", alpha preservation and NaN/Inf rejection\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
