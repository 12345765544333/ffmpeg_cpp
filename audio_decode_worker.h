#pragma once
#include "safe_queue.h"
#include "audioframe.h"
#include <atomic>
#include "ring_buffer.h"
extern "C" {
#include <libavcodec/avcodec.h>
}

class AudioDecodeWorker {
public:
    AudioDecodeWorker(AVCodecContext* dec_ctx, int audio_stream_idx, SafeQueue<AVPacket*>& packet_queue, RingBuffer<AudioFrame>& frame_queue, std::atomic<bool>& read_finished, std::atomic<bool>& decode_finished);
    void operator()();
private:
    AVCodecContext* dec_ctx_;
    int audio_stream_idx_;
    SafeQueue<AVPacket*>& packet_queue_;
    RingBuffer<AudioFrame>& frame_queue_;
    std::atomic<bool>& read_finished_;
    std::atomic<bool>& decode_finished_;
}; 