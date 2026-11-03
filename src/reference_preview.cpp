#include "reference_preview.h"
#include <windows.h>
#include <wincodec.h>
#include <algorithm>
#include <stdexcept>

namespace veil {
namespace {
void checked(HRESULT hr){if(FAILED(hr))throw std::runtime_error("Reference preview encoding failed");}
std::string base64(const unsigned char*p,size_t n){
    constexpr char table[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;out.reserve((n+2)/3*4);
    for(size_t i=0;i<n;i+=3){const unsigned v=(unsigned(p[i])<<16)|(i+1<n?unsigned(p[i+1])<<8:0)|(i+2<n?p[i+2]:0);
        out+=table[(v>>18)&63];out+=table[(v>>12)&63];out+=i+1<n?table[(v>>6)&63]:'=';out+=i+2<n?table[v&63]:'=';}
    return out;
}
}
std::string reference_preview_html(const MatteFrame&f){
    if(!f.width||!f.height||f.pixels.size()!=size_t(f.width)*f.height*4)throw std::runtime_error("Invalid reference preview");
    const double scale=std::min({1.,320./f.width,240./f.height});
    const unsigned w=std::max(1u,unsigned(f.width*scale)),h=std::max(1u,unsigned(f.height*scale));
    std::vector<unsigned char>pixels(size_t(w)*h*4);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        const size_t src=(size_t(uint64_t(y)*f.height/h)*f.width+uint64_t(x)*f.width/w)*4,dst=(size_t(y)*w+x)*4;
        const unsigned a=f.pixels[src+3];pixels[dst+3]=static_cast<unsigned char>(a);
        for(int c=0;c<3;c++)pixels[dst+c]=a?static_cast<unsigned char>(std::min(255u,(unsigned(f.pixels[src+c])*255+a/2)/a)):0;
    }
    const HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(init)&&init!=RPC_E_CHANGED_MODE)checked(init);
    IWICImagingFactory*factory=nullptr;IStream*stream=nullptr;IWICBitmapEncoder*encoder=nullptr;IWICBitmapFrameEncode*frame=nullptr;IPropertyBag2*options=nullptr;
    auto release=[&]{if(options)options->Release();if(frame)frame->Release();if(encoder)encoder->Release();if(stream)stream->Release();if(factory)factory->Release();if(SUCCEEDED(init))CoUninitialize();};
    try{
        checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        checked(CreateStreamOnHGlobal(nullptr,TRUE,&stream));checked(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));
        checked(encoder->Initialize(stream,WICBitmapEncoderNoCache));checked(encoder->CreateNewFrame(&frame,&options));checked(frame->Initialize(options));checked(frame->SetSize(w,h));
        WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;checked(frame->SetPixelFormat(&format));
        if(!IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA))throw std::runtime_error("Unexpected reference preview pixel format");
        checked(frame->WritePixels(h,w*4,static_cast<UINT>(pixels.size()),pixels.data()));checked(frame->Commit());checked(encoder->Commit());
        STATSTG stat{};checked(stream->Stat(&stat,STATFLAG_NONAME));HGLOBAL global=nullptr;checked(GetHGlobalFromStream(stream,&global));
        auto*data=static_cast<unsigned char*>(GlobalLock(global));if(!data)throw std::runtime_error("Reference preview memory unavailable");
        std::string encoded;try{encoded=base64(data,size_t(stat.cbSize.QuadPart));}catch(...){GlobalUnlock(global);throw;}GlobalUnlock(global);
        auto html="<b>등록한 배경</b><br><img alt=\"등록한 배경 미리보기\" width=\""+std::to_string(w)+"\" height=\""+std::to_string(h)+"\" src=\"data:image/png;base64,"+encoded+"\"><br>사람이 보이면 빈 배경을 다시 등록해 주세요.";
        release();return html;
    }catch(...){release();throw;}
}
}
