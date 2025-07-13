#include "video_encode_worker.h"
#include <cstring>

VideoEncodeWorker::VideoEncodeWorker(RingBuffer<VideoFrame>& frame_queue,
                                     SafeQueue<AVPacket*>& encoded_packet_queue,
                                     AVCodecContext* video_enc_ctx,
                                     std::atomic<bool>& decode_finished,
                                     std::atomic<bool>& encode_finished)
    : frame_queue_(frame_queue), encoded_packet_queue_(encoded_packet_queue),
      video_enc_ctx_(video_enc_ctx), decode_finished_(decode_finished), encode_finished_(encode_finished) {}

void VideoEncodeWorker::operator()() {
    SwsContext* sws_ctx = nullptr;
    AVFrame* enc_frame = av_frame_alloc();
    enc_frame->format = video_enc_ctx_->pix_fmt;
    enc_frame->width = video_enc_ctx_->width;
    enc_frame->height = video_enc_ctx_->height;
    av_frame_get_buffer(enc_frame, 0);
    while (!decode_finished_ || !frame_queue_.empty()) {
        VideoFrame vf;
        frame_queue_.pop(vf);
        AVFrame* frame = vf.frame;
        if (frame) {
            if (frame->format != AV_PIX_FMT_YUV420P) {
                if (!sws_ctx) {
                    sws_ctx = sws_getContext(
                        frame->width, frame->height, (AVPixelFormat)frame->format,
                        enc_frame->width, enc_frame->height, AV_PIX_FMT_YUV420P,
                        SWS_BILINEAR, nullptr, nullptr, nullptr);
                }
                av_frame_make_writable(enc_frame);
                sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
                          enc_frame->data, enc_frame->linesize);
                enc_frame->pts = frame->pts;
            } else {
                av_frame_make_writable(enc_frame);
                for (int i = 0; i < 3; ++i) {
                    memcpy(enc_frame->data[i], frame->data[i],
                           (i == 0 ? enc_frame->height : enc_frame->height / 2) * enc_frame->linesize[i]);
                }
                enc_frame->pts = frame->pts;
            }
            if (avcodec_send_frame(video_enc_ctx_, enc_frame) == 0) {
                AVPacket* pkt = av_packet_alloc();
                while (avcodec_receive_packet(video_enc_ctx_, pkt) == 0) {
                    encoded_packet_queue_.push(av_packet_clone(pkt));
                    av_packet_unref(pkt);
                }
                av_packet_free(&pkt);
            }
            av_frame_free(&frame);
        }
    }
    avcodec_send_frame(video_enc_ctx_, nullptr);
    AVPacket* pkt = av_packet_alloc();
    while (avcodec_receive_packet(video_enc_ctx_, pkt) == 0) {
        encoded_packet_queue_.push(av_packet_clone(pkt));
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    if (sws_ctx) sws_freeContext(sws_ctx);
    av_frame_free(&enc_frame);
    encode_finished_ = true;
}
