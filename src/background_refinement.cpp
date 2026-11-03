#include "background_refinement.h"
#include "matting_processing.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace veil {
namespace {
bool same_shape(const MatteFrame&a,const MatteFrame&b){
    return a.width&&a.height&&a.width==b.width&&a.height==b.height&&
        a.pixels.size()==size_t(a.width)*a.height*4&&b.pixels.size()==a.pixels.size();
}
void set_alpha(MatteFrame&result,size_t i,unsigned next){
    const unsigned alpha=result.pixels[i+3];if(next==alpha)return;
    for(int c=0;c<3;c++)result.pixels[i+c]=alpha?uint8_t(std::min(next,unsigned(result.pixels[i+c])*next/alpha)):0;
    result.pixels[i+3]=uint8_t(next);
}
struct PhotoMatch{std::array<int,3>offset{};float noise=0;};
bool match_photo(const MatteFrame&source,const MatteFrame&plate,const MatteFrame&result,PhotoMatch&match){
    std::array<std::array<unsigned,129>,3>hist{};unsigned count=0;
    for(unsigned y=0;y<source.height;y+=4)for(unsigned x=0;x<source.width;x+=4){
        const size_t i=(size_t(y)*source.width+x)*4;
        if(result.pixels[i+3]>12||source.pixels[i+3]!=255||plate.pixels[i+3]!=255)continue;
        count++;for(int c=0;c<3;c++)hist[c][std::clamp(int(source.pixels[i+c])-plate.pixels[i+c],-64,64)+64]++;
    }
    if(count<64)return false;
    for(int c=0;c<3;c++){
        unsigned sum=0;for(int n=0;n<129;n++){sum+=hist[c][n];if(sum>count/2){match.offset[c]=n-64;break;}}
        if(std::abs(match.offset[c])>24)return false;
    }
    unsigned inliers=0;double squared_noise=0;std::array<unsigned,16>tiles{};
    for(unsigned y=0;y<source.height;y+=4)for(unsigned x=0;x<source.width;x+=4){
        const size_t i=(size_t(y)*source.width+x)*4;
        if(result.pixels[i+3]>12||source.pixels[i+3]!=255||plate.pixels[i+3]!=255)continue;
        int residual=0,squared=0;for(int c=0;c<3;c++){const int r=int(source.pixels[i+c])-plate.pixels[i+c]-match.offset[c];residual+=std::abs(r);squared+=r*r;}
        if(residual<=18){inliers++;squared_noise+=squared;tiles[(y*4/source.height)*4+x*4/source.width]++;}
    }
    const unsigned occupied=unsigned(std::count_if(tiles.begin(),tiles.end(),[](unsigned n){return n>=2;}));
    if(inliers*100<count*85||occupied<6)return false;
    match.noise=float(std::sqrt(squared_noise/(3*inliers)));return true;
}
bool background_neighbor(const MatteFrame&source,const MatteFrame&plate,const std::vector<uint8_t>&alpha,
    size_t i,const PhotoMatch&match){
    if(alpha[i/4]>12||source.pixels[i+3]!=255||plate.pixels[i+3]!=255)return false;
    int residual=0;for(int c=0;c<3;c++)residual+=std::abs(int(source.pixels[i+c])-plate.pixels[i+c]-match.offset[c]);
    return residual<=18;
}
}
bool refine_registered_background(const MatteFrame&source,const MatteFrame&plate,MatteFrame&result){
    if(!same_shape(source,plate)||!same_shape(source,result))return false;
    PhotoMatch match;if(!match_photo(source,plate,result,match))return false;
    // Decisions use untouched automatic alpha, never neighbouring corrections.
    const size_t pixels=size_t(source.width)*source.height;
    std::vector<uint8_t>alpha(pixels);for(size_t n=0;n<pixels;n++)alpha[n]=result.pixels[n*4+3];
    for(unsigned y=0;y<source.height;y++)for(unsigned x=0;x<source.width;x++){
        const size_t i=(size_t(y)*source.width+x)*4;const unsigned a=alpha[i/4];
        if(!a||a>=240||source.pixels[i+3]!=255||plate.pixels[i+3]!=255)continue;
        unsigned matching_neighbors=0;bool near_person=false;
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
            if(!dx&&!dy)continue;const int nx=int(x)+dx,ny=int(y)+dy;
            if(nx<0||ny<0||nx>=int(source.width)||ny>=int(source.height))continue;
            const size_t j=(size_t(ny)*source.width+unsigned(nx))*4;
            near_person|=alpha[j/4]>12;
            if(background_neighbor(source,plate,alpha,j,match))matching_neighbors++;
        }
        if(a<=12){
            // Taper to zero removal at12, avoiding the old12->0,13->13 jump.
            // Faint pixels next to predicted hair/body remain intact.
            if(near_person)continue;
            int residual=0;for(int c=0;c<3;c++)residual+=std::abs(int(source.pixels[i+c])-plate.pixels[i+c]-match.offset[c]);
            const float confidence=std::clamp((18-residual)/12.f,0.f,1.f);
            set_alpha(result,i,unsigned(std::lround(a-float(a)*(12-a)/12.f*confidence)));continue;
        }
        if(a<=24||matching_neighbors<2)continue;
        // Shared Matting colour-line estimate (PyMatting, MIT). Recover F
        // from RVM premultiplied output; NEVER substitute observed source I.
        float dot=0,norm=0;std::array<float,3>ib{},fb{};bool clipped=false;
        for(int c=0;c<3;c++){
            const int background=int(plate.pixels[i+c])+match.offset[c];
            if(background<0||background>255){clipped=true;break;}
            ib[c]=int(source.pixels[i+c])-background;
            fb[c]=float(result.pixels[i+c])*255.f/a-background;
            dot+=ib[c]*fb[c];norm+=fb[c]*fb[c];
        }
        if(clipped||norm<=2500)continue;
        const float estimate=dot/(norm+1e-5f),projected=estimate*255.f,difference=projected-a;
        if(estimate<0||estimate>1||std::abs(difference)>=32)continue;
        float error=0;for(int c=0;c<3;c++){const float r=ib[c]-estimate*fb[c];error+=r*r;}
        const float rms=std::sqrt(error/3.f);if(rms>=6)continue;
        // Longitudinal sensor noise can have zero perpendicular residual. Do
        // not disturb a stable model edge inside measured/quantization noise.
        const float uncertainty=std::max(8.f,(std::max(2.f,match.noise)+127.5f/a+.5f)*(255.f*1.7320508f)/std::sqrt(norm));
        if(std::abs(difference)<=uncertainty)continue;
        const float band=std::clamp(std::min((a-24)/24.f,(240-a)/32.f),0.f,1.f);
        const float confidence=band*std::min(1.f,(norm-2500)/7500.f)*(1-rms/6.f)*
            (1-std::abs(difference)/32.f)*std::min(1.f,matching_neighbors/4.f);
        const float beyond_noise=std::copysign(std::abs(difference)-uncertainty,difference);
        const float correction=std::clamp(beyond_noise*.5f*confidence,-8.f,8.f);
        set_alpha(result,i,unsigned(std::clamp(int(std::lround(a+correction)),13,239)));
    }
    return true;
}
bool postprocess_matte(const MatteFrame&source,const MatteFrame*plate,MatteFrame&result,
    const MatteFrame*previous_source,const MatteFrame*previous_automatic,int stability,MatteFrame*automatic_history){
    if(previous_source&&previous_automatic&&same_shape(source,*previous_source)&&same_shape(result,*previous_automatic)){
        stabilize_matte(source.pixels.data(),previous_source->pixels.data(),result.pixels.data(),
            previous_automatic->pixels.data(),result.pixels.size()/4,stability);
    }
    if(automatic_history)*automatic_history=result;
    return plate&&refine_registered_background(source,*plate,result);
}
}
