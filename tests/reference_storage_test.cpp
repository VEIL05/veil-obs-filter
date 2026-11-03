// Exercise the real storage worker at shutdown, without a camera/model/session.
#include "../src/video_matting_filter.cpp"
#include <iostream>

namespace {
MatteFrame reference_fixture(unsigned value){
    MatteFrame frame;frame.width=64;frame.height=48;frame.pixels.resize(64*48*4);
    for(size_t i=0;i<frame.pixels.size();i+=4){frame.pixels[i]=uint8_t(value);frame.pixels[i+1]=83;frame.pixels[i+2]=121;frame.pixels[i+3]=255;}
    return frame;
}
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);}
}
int main(int argc,char**argv){
    if(argc!=3)return 2;
    try{
        const std::string phase=argv[1];const fs::path file=fs::u8path(argv[2])/"generated-background.veilbg";
        if(phase=="write"){
            save_reference(file,reference_fixture(27));
            Filter filter;filter.file=file;filter.reference=std::make_shared<MatteFrame>(reference_fixture(164));
            // The render thread has queued the new plate while the worker is
            // busy. OBS closes before its next loop: the pending write must drain.
            filter.stop=true;filter.save=true;worker(&filter);
            auto loaded=load_reference(file);
            check(loaded&&loaded->pixels==filter.reference->pixels,"Shutdown dropped the queued replacement reference");
        }else if(phase=="read"){
            auto loaded=load_reference(file);
            check(loaded&&loaded->pixels==reference_fixture(164).pixels,"A new process did not restore the saved reference");
            check(make_preview(*loaded).find("data:image/png;base64,")!=std::string::npos,"Restored reference has no thumbnail");
        }else if(phase=="erase"){
            Filter filter;filter.file=file;filter.stop=true;filter.erase=true;worker(&filter);
            check(!fs::exists(file),"Shutdown dropped the explicitly requested deletion");
        }else return 2;
        std::cout<<"PASS reference storage "<<phase<<'\n';return 0;
    }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
