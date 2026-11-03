// Public/generated image sensitivity probe; no camera or OBS settings access.
#include "matting_model.h"
#include "background_refinement.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>

using veil::MatteFrame;
namespace fs=std::filesystem;
int main(int argc,char**argv){
    if(argc!=4)return 2;
    try{
        const fs::path payload=fs::u8path(argv[1]),fixture=fs::u8path(argv[2]),out=fs::u8path(argv[3]);fs::create_directories(out);
        std::vector<uint8_t>photo(320*320*4);std::ifstream input(fixture,std::ios::binary);input.read((char*)photo.data(),photo.size());
        if(input.gcount()!=std::streamsize(photo.size()))throw std::runtime_error("public fixture size");
        const auto runtime=payload/"data/engines/video/bin/64bit/veil-matting-ort.dll";
        const auto models=payload/"data/engines/video/data/models";
        MatteFrame photo_frame;photo_frame.width=640;photo_frame.height=480;photo_frame.pixels.resize(640*480*4);
        for(unsigned y=0;y<480;y++)for(unsigned x=0;x<640;x++){auto i=(size_t(y)*640+x)*4;photo_frame.pixels[i]=photo_frame.pixels[i+1]=photo_frame.pixels[i+2]=90;photo_frame.pixels[i+3]=255;
            if(x>=80&&x<560)std::copy_n(photo.data()+(size_t(y*320/480)*320+(x-80)*320/480)*4,4,photo_frame.pixels.data()+i);}
        veil::MattingModel auto_model(runtime,models/"rvm.onnx",false,false);
        MatteFrame truth;for(int n=0;n<8;n++)truth=auto_model.process(photo_frame);
        MatteFrame plate=photo_frame;
        for(unsigned y=0;y<480;y++)for(unsigned x=0;x<640;x++){auto i=(size_t(y)*640+x)*4;int pattern=((x/19+y/23)%2)*9;
            plate.pixels[i]=uint8_t(100+pattern);plate.pixels[i+1]=uint8_t(108+pattern);plate.pixels[i+2]=uint8_t(116+pattern);plate.pixels[i+3]=255;}
        MatteFrame composite=plate;
        for(size_t i=0;i<composite.pixels.size();i+=4)for(int c=0;c<3;c++)composite.pixels[i+c]=uint8_t(std::min(255u,unsigned(truth.pixels[i+c])+unsigned(plate.pixels[i+c])*(255-truth.pixels[i+3])/255));
        std::ofstream report(out/"metrics.csv");report<<"engine,case,core_missing_fraction,background_alpha_mean,edge_temporal_mae\n";
        for(const std::string engine:{"automatic","registered_old","registered_recurrent"}){
            veil::MattingModel model(runtime,models/(engine=="registered_old"?"bmv2.onnx":"rvm.onnx"),engine=="registered_old",false);
            for(int exposure:{0,12}){
                model.reset();double missing=0,background=0,flicker=0;size_t cores=0,backgrounds=0,edges=0;MatteFrame prev;
                for(int n=0;n<14;n++){
                    MatteFrame frame=composite;
                    for(size_t i=0;i<frame.pixels.size();i+=4)for(int c=0;c<3;c++){int noise=(int((i*13+n*97+unsigned(c)*3)%7)-3);int shift=exposure+(exposure?((n%2)*4-2):0);
                        frame.pixels[i+c]=uint8_t(std::clamp(int(frame.pixels[i+c])+shift+noise,0,255));}
                    auto result=model.process(frame,engine=="registered_old"?&plate:nullptr);
                    if(engine=="registered_recurrent")veil::refine_registered_background(frame,plate,result);
                    if(n>=4)for(size_t i=3;i<result.pixels.size();i+=4){const auto a=truth.pixels[i];
                        if(a>=250){cores++;missing+=result.pixels[i]<200;}
                        if(a<=5){backgrounds++;background+=result.pixels[i];}
                        if(a>5&&a<250&&!prev.pixels.empty()){edges++;flicker+=std::abs(int(result.pixels[i])-int(prev.pixels[i]));}}
                    prev=std::move(result);
                }
                report<<engine<<','<<(exposure?"exposure_jitter":"noise")<<','<<missing/cores<<','<<background/backgrounds<<','<<flicker/edges<<'\n';
                std::ofstream(out/(engine+"-"+std::to_string(exposure)+".bgra"),std::ios::binary).write((char*)prev.pixels.data(),prev.pixels.size());
            }
        }
        std::cout<<"PASS sensitivity probe completed; reference alpha is RVM-derived, not independently annotated ground truth\n";return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
