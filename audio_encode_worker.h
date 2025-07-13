#pragma once
#include "audioframe.h"
#include "ring_buffer.h"
#include "safe_queue.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}
#include <atomic>

class AudioEncodeWorker {
public:
    AudioEncodeWorker(RingBuffer<AudioFrame>& frame_queue,
                      SafeQueue<AVPacket*>& encoded_packet_queue,
                      AVCodecContext* enc_ctx,
                      std::atomic<bool>& decode_finished,
                      std::atomic<bool>& encode_finished);
    void operator()();
private:
    RingBuffer<AudioFrame>& frame_queue_;
    SafeQueue<AVPacket*>& encoded_packet_queue_;
    AVCodecContext* enc_ctx_;
    std::atomic<bool>& decode_finished_;
    std::atomic<bool>& encode_finished_;
}; 