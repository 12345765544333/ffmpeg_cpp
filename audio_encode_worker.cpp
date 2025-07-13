#include "audio_encode_worker.h"
#include <cstring>
#include <iostream>

AudioEncodeWorker::AudioEncodeWorker(RingBuffer<AudioFrame>& frame_queue,
                                     SafeQueue<AVPacket*>& encoded_packet_queue,
                                     AVCodecContext* enc_ctx,
                                     std::atomic<bool>& decode_finished,
                                     std::atomic<bool>& encode_finished)
    : frame_queue_(frame_queue), encoded_packet_queue_(encoded_packet_queue),
      enc_ctx_(enc_ctx), decode_finished_(decode_finished), encode_finished_(encode_finished) {}

void AudioEncodeWorker::operator()() {
    std::cout << "[AudioEncodeWorker] Start audio encoding thread." << std::endl;
    SwrContext* swr_ctx = nullptr;
    AVFrame* resampled_frame = av_frame_alloc();
    resampled_frame->format = enc_ctx_->sample_fmt;
    av_channel_layout_copy(&resampled_frame->ch_layout, &enc_ctx_->ch_layout);
    resampled_frame->sample_rate = enc_ctx_->sample_rate;
    resampled_frame->nb_samples = enc_ctx_->frame_size > 0 ? enc_ctx_->frame_size : 1024;
    av_frame_get_buffer(resampled_frame, 0);

    while (!decode_finished_ || !frame_queue_.empty()) {
        AudioFrame af;
        frame_queue_.pop(af);
        AVFrame* in_frame = af.frame;
        if (in_frame) {
            // 初始化重采样器
            if (!swr_ctx) {
                swr_ctx = swr_alloc();
                swr_alloc_set_opts2(
                    &swr_ctx,
                    &enc_ctx_->ch_layout, enc_ctx_->sample_fmt, enc_ctx_->sample_rate,
                    &in_frame->ch_layout, (AVSampleFormat)in_frame->format, in_frame->sample_rate,
                    0, nullptr
                );
                swr_init(swr_ctx);
                std::cout << "[AudioEncodeWorker] SwrContext initialized for resampling." << std::endl;
            }
            // 重采样
            av_frame_make_writable(resampled_frame);
            int out_samples = swr_convert(
                swr_ctx,
                resampled_frame->data, resampled_frame->nb_samples,
                (const uint8_t**)in_frame->data, in_frame->nb_samples
            );
            resampled_frame->nb_samples = out_samples;
            resampled_frame->pts = in_frame->pts;
            //std::cout << "[AudioEncodeWorker] Resampled frame: nb_samples=" << out_samples << ", format=" << resampled_frame->format << ", sample_rate=" << resampled_frame->sample_rate << std::endl;

            // 编码
            if (avcodec_send_frame(enc_ctx_, resampled_frame) == 0) {
                AVPacket* pkt = av_packet_alloc();
                while (avcodec_receive_packet(enc_ctx_, pkt) == 0) {
                    //std::cout << "[AudioEncodeWorker] Encoded audio packet: size=" << pkt->size << std::endl;
                    encoded_packet_queue_.push(av_packet_clone(pkt));
                    av_packet_unref(pkt);
                }
                av_packet_free(&pkt);
            }
            av_frame_free(&in_frame);
        }
    }
    // flush
    avcodec_send_frame(enc_ctx_, nullptr);
    AVPacket* pkt = av_packet_alloc();
    while (avcodec_receive_packet(enc_ctx_, pkt) == 0) {
        //std::cout << "[AudioEncodeWorker] Flushed encoded audio packet: size=" << pkt->size << std::endl;
        encoded_packet_queue_.push(av_packet_clone(pkt));
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    if (swr_ctx) swr_free(&swr_ctx);
    av_frame_free(&resampled_frame);
    std::cout << "[AudioEncodeWorker] Audio encoding finished." << std::endl;
    encode_finished_ = true;
} 