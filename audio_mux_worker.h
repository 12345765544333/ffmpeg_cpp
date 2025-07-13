#pragma once
#include "safe_queue.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#include <atomic>

class AudioMuxWorker {
public:
    AudioMuxWorker(SafeQueue<AVPacket*>& encoded_packet_queue,
                   AVFormatContext* out_fmt_ctx,
                   AVStream* out_stream,
                   AVCodecContext* enc_ctx,
                   std::atomic<bool>& encode_finished);
    void operator()();
private:
    SafeQueue<AVPacket*>& encoded_packet_queue_;
    AVFormatContext* out_fmt_ctx_;
    AVStream* out_stream_;
    AVCodecContext* enc_ctx_;
    std::atomic<bool>& encode_finished_;
}; 