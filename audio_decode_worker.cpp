#include "audio_decode_worker.h"
#include <iostream>

AudioDecodeWorker::AudioDecodeWorker(AVCodecContext* dec_ctx, int audio_stream_idx, SafeQueue<AVPacket*>& packet_queue, RingBuffer<AudioFrame>& frame_queue, std::atomic<bool>& read_finished, std::atomic<bool>& decode_finished)
    : dec_ctx_(dec_ctx), audio_stream_idx_(audio_stream_idx), packet_queue_(packet_queue), frame_queue_(frame_queue), read_finished_(read_finished), decode_finished_(decode_finished) {}

void AudioDecodeWorker::operator()() {
    //std::cout << "[AudioDecodeWorker] Start audio decoding thread." << std::endl;
    while (!read_finished_ || !packet_queue_.empty()) {
        AVPacket* pkt = nullptr;
        if (packet_queue_.pop(pkt)) {
            if (pkt) {
                // 检查是否为音频流包
                if (pkt->stream_index == audio_stream_idx_) {
                    // 发送包到解码器
                    if (avcodec_send_packet(dec_ctx_, pkt) == 0) {
                        AVFrame* frame = av_frame_alloc();
                        while (avcodec_receive_frame(dec_ctx_, frame) == 0) {
                            // 解码得到音频帧，推入队列
                            //std::cout << "[AudioDecodeWorker] Decoded audio frame: nb_samples=" << frame->nb_samples << ", format=" << frame->format << ", sample_rate=" << frame->sample_rate << std::endl;
                            frame_queue_.push(AudioFrame(frame));
                            frame = av_frame_alloc();
                        }
                        av_frame_free(&frame);
                    }
                }
                av_packet_free(&pkt);
            }
        }
    }
    std::cout << "[AudioDecodeWorker] Audio decoding finished." << std::endl;
    decode_finished_ = true;
} 