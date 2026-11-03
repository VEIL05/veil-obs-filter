#include "matting_processing.h"
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

void require(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
    std::mt19937 random(718927);
    for(size_t n:{size_t(0),size_t(1),size_t(3),size_t(4),size_t(7),size_t(257),size_t(1920*1080)}){
        std::vector<uint8_t>s(n*4),p(n*4),r(n*4),old(n*4);
        for(size_t i=0;i<n;i++)for(int c=0;c<4;c++){
            s[i*4+c]=uint8_t(random());p[i*4+c]=uint8_t(std::clamp(int(s[i*4+c])+int(random()%13)-6,0,255));
            r[i*4+c]=uint8_t(random());old[i*4+c]=uint8_t(std::clamp(int(r[i*4+c])+int(random()%67)-33,0,255));
        }
        for(int alpha:{-1,0,1,127,254,255}){
            auto input=s;if(alpha>=0)for(size_t i=0;i<n;i++)input[i*4+3]=uint8_t(alpha);
            std::vector<float>expected(n*3),actual(n*3);
            for(size_t i=0;i<n;i++){
                const float scale=input[i*4+3]?1.f/input[i*4+3]:0.f;
                for(int c=0;c<3;c++)expected[size_t(c)*n+i]=std::clamp(input[i*4+2-c]*scale,0.f,1.f);
            }
            veil::planar_bgra(input.data(),n,actual.data());
            require(!n||!std::memcmp(expected.data(),actual.data(),n*3*sizeof(float)),"Planar conversion differs from original scalar floats");
        }
        for(int stability:{0,1,25,75,100}){
            auto expected=r,actual=r;const float weight=stability*.003f;
            for(size_t i=0;i<n;i++){
                int motion=0;for(int c=0;c<3;c++)motion+=std::abs(int(s[i*4+c])-int(p[i*4+c]));
                if(motion<=12&&std::abs(int(r[i*4+3])-int(old[i*4+3]))<32)
                    for(int c=0;c<4;c++)expected[i*4+c]=uint8_t(r[i*4+c]*(1-weight)+old[i*4+c]*weight+.5f);
            }
            veil::stabilize_matte(s.data(),p.data(),actual.data(),old.data(),n,stability);
            require(expected==actual,"Temporal blend differs from original scalar bytes");
        }
    }
    std::cout<<"PASS exact scalar/SSE2 planar floats and temporal BGRA, alpha0..255, motion gates, odd sizes, full1080\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
