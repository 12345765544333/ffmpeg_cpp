#pragma once
#include "safe_queue.h"
#include "ring_buffer.h"
#include "videoframe.h"
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <atomic>

class VideoDecodeWorker {
public:
    VideoDecodeWorker(SafeQueue<AVPacket*>& packet_queue,
                      RingBuffer<VideoFrame>& frame_queue,
                      AVCodecContext* dec_ctx,
                      std::atomic<bool>& demux_finished,
                      std::atomic<bool>& decode_finished);
    void operator()(); // 线程体
private:
    SafeQueue<AVPacket*>& packet_queue_;
    RingBuffer<VideoFrame>& frame_queue_;
    AVCodecContext* dec_ctx_;
    std::atomic<bool>& demux_finished_;
    std::atomic<bool>& decode_finished_;
};
