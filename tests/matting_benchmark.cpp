#include "matting_model.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
int main(int argc,char**argv){
 if(argc!=7)return 2;
 namespace fs=std::filesystem;const fs::path payload=fs::u8path(argv[1]),out=fs::u8path(argv[2]);
 const unsigned w=std::stoul(argv[4]),h=std::stoul(argv[5]);const bool gpu=std::stoi(argv[6])!=0;
 try{
  fs::create_directories(out);std::vector<uint8_t>photo(320*320*4);std::ifstream input(fs::u8path(argv[3]),std::ios::binary);input.read((char*)photo.data(),photo.size());
  if(input.gcount()!=std::streamsize(photo.size()))throw std::runtime_error("fixture size");
  veil::MatteFrame frame;frame.width=w;frame.height=h;frame.pixels.resize(size_t(w)*h*4);const unsigned left=(w-h)/2;
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){const auto i=(size_t(y)*w+x)*4;frame.pixels[i]=frame.pixels[i+1]=frame.pixels[i+2]=90;frame.pixels[i+3]=255;if(x>=left&&x<left+h)std::copy_n(photo.data()+(size_t(y*320/h)*320+(x-left)*320/h)*4,4,frame.pixels.data()+i);}
  veil::MattingModel model(payload/"data/engines/video/bin/64bit/veil-matting-ort.dll",payload/"data/engines/video/data/models/rvm.onnx",false,gpu);
  std::vector<double>times;veil::MatteFrame result;
  for(int n=0;n<24;n++){const auto begin=std::chrono::steady_clock::now();result=model.process(frame);const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();if(n>=4)times.push_back(ms);}
  std::sort(times.begin(),times.end());
  std::ofstream(out/"result.bgra",std::ios::binary).write((char*)result.pixels.data(),result.pixels.size());
  std::ofstream report(out/"timing.json");report<<"{\"width\":"<<w<<",\"height\":"<<h<<",\"gpu\":"<<(gpu?"true":"false")<<",\"median_ms\":"<<times[times.size()/2]<<",\"p95_ms\":"<<times[size_t(times.size()*.95)-1]<<"}\n";
  std::cout<<"PASS "<<w<<"x"<<h<<" median="<<times[times.size()/2]<<"ms p95="<<times[size_t(times.size()*.95)-1]<<"ms\n";
  return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
