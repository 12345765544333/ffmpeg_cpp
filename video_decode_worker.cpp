#include "video_decode_worker.h"
#include<iostream>
VideoDecodeWorker::VideoDecodeWorker(SafeQueue<AVPacket*>& packet_queue,
                                     RingBuffer<VideoFrame>& frame_queue,
                                     AVCodecContext* dec_ctx,
                                     std::atomic<bool>& demux_finished,
                                     std::atomic<bool>& decode_finished)
    : packet_queue_(packet_queue), frame_queue_(frame_queue), dec_ctx_(dec_ctx),
      demux_finished_(demux_finished), decode_finished_(decode_finished) {}

void VideoDecodeWorker::operator()() {
    int64_t next_pts = 0;
    while (!demux_finished_ || !packet_queue_.empty()) {
        AVPacket* pkt = nullptr;
        if (packet_queue_.pop(pkt)) {
            if (avcodec_send_packet(dec_ctx_, pkt) == 0) {
                while (true) {
                    AVFrame* frame = av_frame_alloc();
                    if (frame->format == AV_PIX_FMT_CUDA) {
                        std::cout << "使用了 CUDA 硬件解码。" << std::endl;
                    }
                    if (!frame) break;
                    int ret = avcodec_receive_frame(dec_ctx_, frame);
                    if (ret == 0) {
                        frame->pts = next_pts++;
                        frame_queue_.push(VideoFrame(av_frame_clone(frame)));
                        av_frame_free(&frame);
                    } else {
                        av_frame_free(&frame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                            break;
                        else break;
                    }
                }
            }
            av_packet_free(&pkt);
        }
    }
    decode_finished_ = true;
}
