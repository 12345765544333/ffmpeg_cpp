#pragma once
#include "safe_queue.h"
#include <string>
#include <atomic>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class Demuxer {
public:
    Demuxer(AVFormatContext* fmt_ctx,
            SafeQueue<AVPacket*>& video_queue, int& video_stream_idx,
            SafeQueue<AVPacket*>* audio_queue = nullptr, int* audio_stream_idx = nullptr);
    ~Demuxer();
    // 只负责解复用
    void demux(std::atomic<bool>* finished_flag = nullptr);
    void operator()(std::atomic<bool>* finished_flag = nullptr);
    AVFormatContext* get_fmt_ctx() const { return fmt_ctx_; }
    int get_video_stream_idx() const { return video_stream_idx_; }
    int get_audio_stream_idx() const { return audio_stream_idx_ ? *audio_stream_idx_ : -1; }
private:
    AVFormatContext* fmt_ctx_ = nullptr;
    SafeQueue<AVPacket*>& video_queue_;
    int& video_stream_idx_;
    SafeQueue<AVPacket*>* audio_queue_;
    int* audio_stream_idx_;
};
