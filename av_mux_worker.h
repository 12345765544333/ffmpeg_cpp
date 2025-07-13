#pragma once
#include "safe_queue.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#include <atomic>

class AVMuxWorker {
public:
    AVMuxWorker(SafeQueue<AVPacket*>& video_queue,
                SafeQueue<AVPacket*>& audio_queue,
                AVFormatContext* out_fmt_ctx,
                AVStream* video_stream,
                AVCodecContext* video_enc_ctx,
                AVStream* audio_stream,
                AVCodecContext* audio_enc_ctx,
                std::atomic<bool>& video_encode_finished,
                std::atomic<bool>& audio_encode_finished);
    void operator()();
private:
    SafeQueue<AVPacket*>& video_queue_;
    SafeQueue<AVPacket*>& audio_queue_;
    AVFormatContext* out_fmt_ctx_;
    AVStream* video_stream_;
    AVCodecContext* video_enc_ctx_;
    AVStream* audio_stream_;
    AVCodecContext* audio_enc_ctx_;
    std::atomic<bool>& video_encode_finished_;
    std::atomic<bool>& audio_encode_finished_;
}; 