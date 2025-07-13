#pragma once
#include "ring_buffer.h"
#include "videoframe.h"
#include "safe_queue.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#include <atomic>

class VideoEncodeWorker {
public:
    VideoEncodeWorker(RingBuffer<VideoFrame>& frame_queue,
                      SafeQueue<AVPacket*>& encoded_packet_queue,
                      AVCodecContext* video_enc_ctx,
                      std::atomic<bool>& decode_finished,
                      std::atomic<bool>& encode_finished);
    void operator()(); // 线程体
private:
    RingBuffer<VideoFrame>& frame_queue_;
    SafeQueue<AVPacket*>& encoded_packet_queue_;
    AVCodecContext* video_enc_ctx_;
    std::atomic<bool>& decode_finished_;
    std::atomic<bool>& encode_finished_;
};
