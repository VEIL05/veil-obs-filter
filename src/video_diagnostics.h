#pragma once
#include <obs-module.h>
#include <util/platform.h>
#include <atomic>
#include <algorithm>
#include <cstdint>

namespace veil {
// Bounded metadata-only tracing. Never retain, modify, or log camera pixels.
// filter_video observes the frame in place and returns the identical pointer;
// obs_source_get_frame is deliberately not used because it consumes a frame.
struct VideoDiagnostics {
    std::atomic<uint64_t> source_frames{0},source_changes{0},source_seen{0},source_stamp{0},source_hash{0};
    // This clock also gates processing: a rendered cached texture is not a new
    // camera frame. Keep it active independently of heartbeat logging.
    std::atomic<uint64_t> source_epoch{0};
    std::atomic<unsigned> source_width{0},source_height{0},source_format{0};
    std::atomic<uint64_t> captures{0},capture_changes{0},capture_hash{0},capture_alpha{0};
    std::atomic<uint64_t> renders{0},ticks{0},begins_failed{0},maps_failed{0},targets_failed{0};
    std::atomic<uint64_t> work_started{0},work_done{0},work_sequence{0},done_sequence{0},work_start_ns{0},work_ms{0},errors{0};
    std::atomic<uint64_t> render_start{0},render_ms{0},max_render_ms{0},last_beat{0};
    std::atomic<int> phase{0},last_enabled{-1};
    static uint32_t plane_bytes(const obs_source_frame&f){
        switch(f.format){
        case VIDEO_FORMAT_RGBA:case VIDEO_FORMAT_BGRA:case VIDEO_FORMAT_BGRX:case VIDEO_FORMAT_AYUV:case VIDEO_FORMAT_R10L:return f.width*4;
        case VIDEO_FORMAT_BGR3:return f.width*3;
        case VIDEO_FORMAT_YVYU:case VIDEO_FORMAT_YUY2:case VIDEO_FORMAT_UYVY:
        case VIDEO_FORMAT_I010:case VIDEO_FORMAT_P010:case VIDEO_FORMAT_I210:case VIDEO_FORMAT_I412:
        case VIDEO_FORMAT_YA2L:case VIDEO_FORMAT_P216:case VIDEO_FORMAT_P416:return f.width*2;
        case VIDEO_FORMAT_V210:return ((f.width+5)/6)*16;
        default:return f.width;
        }
    }
    static uint64_t sample(const uint8_t*data,uint32_t stride,uint32_t rows,uint32_t bytes){
        bytes=std::min(bytes,stride);if(!data||!bytes||!rows)return 0;uint64_t h=1469598103934665603ULL;
        for(unsigned y=1;y<=8;y++)for(unsigned x=1;x<=8;x++){h^=data[size_t(uint64_t(y)*rows/9)*stride+uint64_t(x)*bytes/9];h*=1099511628211ULL;}return h;
    }
    obs_source_frame*observe(obs_source_frame*f){
        if(f){const auto now=os_gettime_ns(),seen=source_seen.load(),stamp=source_stamp.load();
            if(!seen||(now>seen&&now-seen>500000000ULL)||f->timestamp<stamp)source_epoch++;
            const uint64_t h=sample(f->data[0],f->linesize[0],f->height,plane_bytes(*f)),old=source_hash.exchange(h);
            if(source_frames.fetch_add(1)&&h!=old)source_changes++;source_seen=now;source_stamp=f->timestamp;}
        if(f){source_width=f->width;source_height=f->height;source_format=unsigned(f->format);}
        return f;
    }
    bool waiting_for_input(uint64_t now)const{const auto seen=source_seen.load();return !seen||(now>seen&&now-seen>500000000ULL);}
    static bool async_input(obs_source_t*filter){auto*parent=obs_filter_get_parent(filter);return parent&&(obs_source_get_output_flags(parent)&OBS_SOURCE_ASYNC);}
    void capture(const uint8_t*data,uint32_t stride,unsigned w,unsigned h){
        const auto hash=sample(data,stride,h,w*4),old=capture_hash.exchange(hash);if(captures.fetch_add(1)&&hash!=old)capture_changes++;
        unsigned sum=0;for(unsigned y=1;y<=8;y++)for(unsigned x=1;x<=8;x++)sum+=data[size_t(uint64_t(y)*h/9)*stride+(uint64_t(x)*w/9)*4+3];capture_alpha=sum/64;
    }
    void begin_work(uint64_t sequence){work_started++;work_sequence=sequence;work_start_ns=os_gettime_ns();}
    void finish_work(uint64_t sequence){work_done++;done_sequence=sequence;work_ms=(os_gettime_ns()-work_start_ns.load())/1000000;}
    static uint64_t age_ms(uint64_t now,uint64_t then){return then&&now>then?(now-then)/1000000:0;}
    void beat(const char*role,obs_source_t*source){
        const uint64_t now=os_gettime_ns();auto previous=last_beat.load();if(now-previous<5000000000ULL||!last_beat.compare_exchange_strong(previous,now))return;
        const int enabled=obs_source_enabled(source)?1:0;if(last_enabled.exchange(enabled)==0&&!enabled)return;
        const auto seen=source_seen.load(),start=render_start.load(),ws=work_start_ns.load();
        blog(LOG_INFO,"[VEIL trace][%s %.8s] enabled=%d source=%llu changed=%llu source_age_ms=%lld source_ts=%llu size=%ux%u format=%u render=%llu tick=%llu phase=%d render_ms=%llu max_render_ms=%llu render_age_ms=%llu capture=%llu changed=%llu alpha=%llu begin_fail=%llu map_fail=%llu target_fail=%llu work=%llu/%llu seq=%llu/%llu work_ms=%llu work_age_ms=%llu errors=%llu",
            role,obs_source_get_uuid(source),enabled,(unsigned long long)source_frames.load(),(unsigned long long)source_changes.load(),seen?(long long)age_ms(now,seen):-1LL,(unsigned long long)source_stamp.load(),source_width.load(),source_height.load(),source_format.load(),
            (unsigned long long)renders.load(),(unsigned long long)ticks.load(),phase.load(),(unsigned long long)render_ms.load(),(unsigned long long)max_render_ms.load(),(unsigned long long)age_ms(now,start),
            (unsigned long long)captures.load(),(unsigned long long)capture_changes.load(),(unsigned long long)capture_alpha.load(),(unsigned long long)begins_failed.load(),(unsigned long long)maps_failed.load(),(unsigned long long)targets_failed.load(),
            (unsigned long long)work_started.load(),(unsigned long long)work_done.load(),(unsigned long long)work_sequence.load(),(unsigned long long)done_sequence.load(),(unsigned long long)work_ms.load(),(unsigned long long)age_ms(now,ws),(unsigned long long)errors.load());
    }
};
struct RenderTraceScope {
    VideoDiagnostics&d;uint64_t start=os_gettime_ns();
    explicit RenderTraceScope(VideoDiagnostics&diag):d(diag){d.renders++;d.render_start=start;d.phase=1;}
    ~RenderTraceScope(){const auto ms=(os_gettime_ns()-start)/1000000;d.render_ms=ms;auto old=d.max_render_ms.load();while(ms>old&&!d.max_render_ms.compare_exchange_weak(old,ms)){}d.phase=0;}
};
struct WorkTraceScope {
    VideoDiagnostics&d;uint64_t sequence;
    WorkTraceScope(VideoDiagnostics&diag,uint64_t seq):d(diag),sequence(seq){d.begin_work(seq);}
    ~WorkTraceScope(){d.finish_work(sequence);}
};
}
