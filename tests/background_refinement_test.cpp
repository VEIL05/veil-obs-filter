#include "background_refinement.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);}
int main(){try{
    veil::MatteFrame source,plate,result;source.width=plate.width=result.width=64;source.height=plate.height=result.height=64;
    source.pixels.resize(64*64*4);plate.pixels.resize(source.pixels.size());result.pixels.resize(source.pixels.size());
    for(size_t i=0;i<source.pixels.size();i+=4){for(int c=0;c<3;c++){source.pixels[i+c]=118;plate.pixels[i+c]=110;result.pixels[i+c]=4;}source.pixels[i+3]=plate.pixels[i+3]=255;result.pixels[i+3]=8;}
    // Confident body and translucent edges must be identical after plate cleanup.
    for(size_t i=0;i<16*4;i+=4){const auto a=uint8_t(13+i/4*16);result.pixels[i]=std::min(uint8_t(9),a);result.pixels[i+1]=std::min(uint8_t(17),a);result.pixels[i+2]=std::min(uint8_t(31),a);result.pixels[i+3]=a;}
    // A faint real detail whose colour differs from the plate must remain too.
    const size_t detail=20*4;source.pixels[detail]=220;result.pixels[detail]=8;
    const auto original=result.pixels;
    check(veil::refine_registered_background(source,plate,result),"Matched plate rejected");
    for(size_t i=0;i<16*4;i++)check(result.pixels[i]==original[i],"Body/edge altered by registered-background cleanup");
    for(int c=0;c<4;c++)check(result.pixels[detail+c]==original[detail+c],"Nonmatching faint detail removed");
    check(result.pixels[100*4+3]<original[100*4+3],"Matching faint background residue was not reduced");
    for(size_t i=0;i<result.pixels.size();i+=4)for(int c=0;c<3;c++)check(result.pixels[i+c]<=result.pixels[i+3],"Cleanup broke premultiplied colour");
    auto before=result.pixels;plate.width=32;check(!veil::refine_registered_background(source,plate,result)&&result.pixels==before,"Size mismatch changed output");plate.width=64;
    for(size_t i=0;i<source.pixels.size();i+=4)for(int c=0;c<3;c++)source.pixels[i+c]=180;
    check(!veil::refine_registered_background(source,plate,result)&&result.pixels==before,"Large lighting mismatch changed output");
    // Known alpha/foreground/background, independent of either neural model.
    auto fixture=[](unsigned predicted,unsigned truth,int noise){
        std::array<veil::MatteFrame,3>v;
        for(auto&f:v){f.width=f.height=64;f.pixels.resize(64*64*4);}
        for(size_t i=0;i<v[0].pixels.size();i+=4){
            for(int c=0;c<3;c++)v[0].pixels[i+c]=v[1].pixels[i+c]=100;
            v[0].pixels[i+3]=v[1].pixels[i+3]=255;
        }
        for(unsigned y=8;y<56;y++){
            const size_t i=(size_t(y)*64+32)*4;
            for(int c=0;c<3;c++){
                v[0].pixels[i+c]=uint8_t(std::clamp(int(std::lround((200.f*truth+100.f*(255-truth))/255))+noise,0,255));
                v[2].pixels[i+c]=uint8_t(std::lround(200.f*predicted/255));
            }
            v[2].pixels[i+3]=uint8_t(predicted);
        }
        return v;
    };
    const size_t edge=(20*64+32)*4;
    double automatic_error=0,registered_error=0;
    for(unsigned predicted:{108u,148u}){
        auto v=fixture(predicted,128,0);const auto before=v[2].pixels;
        check(veil::refine_registered_background(v[0],v[1],v[2]),"Known good plate rejected");
        automatic_error+=std::abs(int(predicted)-128);registered_error+=std::abs(int(v[2].pixels[edge+3])-128);
        check(std::abs(int(v[2].pixels[edge+3])-int(predicted))<=8,"Photo correction exceeded bound");
    }
    check(registered_error<automatic_error,"Known soft edge did not improve against independent alpha truth");
    for(int noise:{-3,3}){
        auto v=fixture(128,128,noise);auto before=v[2].pixels;
        veil::refine_registered_background(v[0],v[1],v[2]);
        check(v[2].pixels==before,"Longitudinal colour noise destabilized an already correct edge");
    }
    // The old threshold expanded alpha12/13 noise to0/13. No extra jump now.
    std::array<int,2>tail{};for(unsigned a:{12u,13u}){
        auto v=fixture(a,a,0);for(int c=0;c<3;c++)v[0].pixels[edge+c]=100;
        veil::refine_registered_background(v[0],v[1],v[2]);tail[a-12]=v[2].pixels[edge+3];
    }
    check(std::abs(tail[1]-tail[0])<=2,"Alpha12/13 cutoff amplified shimmer");
    // Actual worker postprocess preserves AUTOMATIC history, not photo output.
    auto first=fixture(8,0,0);veil::MatteFrame history;
    veil::postprocess_matte(first[0],&first[1],first[2],nullptr,nullptr,75,&history);
    check(history.pixels[edge+3]==8,"Photo result leaked into automatic temporal history");
    auto second=fixture(16,16,0),automatic=second;
    veil::postprocess_matte(automatic[0],nullptr,automatic[2],&first[0],&history,75,nullptr);
    veil::MatteFrame next_history;
    veil::postprocess_matte(second[0],&second[1],second[2],&first[0],&history,75,&next_history);
    check(next_history.pixels==automatic[2].pixels,"Registered history differs from automatic path");
    check(second[2].pixels[edge+3]>12,"Thin detail eroded through photo feedback");
    // Wrong textured plate must return the exact original automatic output.
    auto wrong=fixture(108,128,0);for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
        const size_t i=(size_t(y)*64+x)*4;for(int c=0;c<3;c++)wrong[1].pixels[i+c]=((x/4+y/4)%2)?65:135;
    }
    auto unchanged=wrong[2].pixels;
    check(!veil::refine_registered_background(wrong[0],wrong[1],wrong[2])&&wrong[2].pixels==unchanged,"Wrong photo changed automatic output");
    // Similar foreground/background colours offer no reliable alpha evidence.
    auto camouflage=fixture(108,128,0);for(unsigned y=8;y<56;y++)for(int c=0;c<3;c++){
        const size_t i=(size_t(y)*64+32)*4;
        camouflage[0].pixels[i+c]=100;camouflage[2].pixels[i+c]=uint8_t(std::lround(100.f*108/255));
    }
    unchanged=camouflage[2].pixels;veil::refine_registered_background(camouflage[0],camouflage[1],camouflage[2]);
    check(camouflage[2].pixels==unchanged,"Camouflaged detail altered without colour separation");
    // Moving/disoccluded diagonal detail already correct in this frame stays so;
    // reference processing never attaches a previous position's alpha.
    for(unsigned n=0;n<8;n++){
        auto moving=fixture(128,128,0);for(unsigned y=8;y<56;y++){
            size_t from=(size_t(y)*64+32)*4,to=(size_t(y)*64+24+n)*4;
            for(int c=0;c<4;c++){std::swap(moving[0].pixels[from+c],moving[0].pixels[to+c]);std::swap(moving[2].pixels[from+c],moving[2].pixels[to+c]);}
        }
        unchanged=moving[2].pixels;veil::refine_registered_background(moving[0],moving[1],moving[2]);
        check(moving[2].pixels==unchanged,"Correct moving thin detail gained a photo trail");
    }
    std::cout<<"PASS independent edge truth MAE "<<automatic_error/2<<" -> "<<registered_error/2<<"; noise, cutoff, feedback, wrong-photo, camouflage, motion canaries\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
