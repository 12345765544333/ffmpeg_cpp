 #include "av_mux_worker.h"
#include <iostream>
#include <thread>

AVMuxWorker::AVMuxWorker(SafeQueue<AVPacket*>& video_queue,
                         SafeQueue<AVPacket*>& audio_queue,
                         AVFormatContext* out_fmt_ctx,
                         AVStream* video_stream,
                         AVCodecContext* video_enc_ctx,
                         AVStream* audio_stream,
                         AVCodecContext* audio_enc_ctx,
                         std::atomic<bool>& video_encode_finished,
                         std::atomic<bool>& audio_encode_finished)
    : video_queue_(video_queue), audio_queue_(audio_queue),
      out_fmt_ctx_(out_fmt_ctx),
      video_stream_(video_stream), video_enc_ctx_(video_enc_ctx),
      audio_stream_(audio_stream), audio_enc_ctx_(audio_enc_ctx),
      video_encode_finished_(video_encode_finished), audio_encode_finished_(audio_encode_finished) {}

void AVMuxWorker::operator()() {
    std::cout << "[AVMuxWorker] Start muxing video and audio..." << std::endl;
    AVPacket* video_pkt = nullptr;
    AVPacket* audio_pkt = nullptr;
    while (!video_queue_.empty() || !audio_queue_.empty()||!video_encode_finished_||!audio_encode_finished_) {
        // peek 不 pop，先比较时间戳
        if (!video_pkt && !video_queue_.empty()) {
            video_queue_.pop(video_pkt);
            av_packet_rescale_ts(video_pkt, video_enc_ctx_->time_base, video_stream_->time_base);
            video_pkt->stream_index = video_stream_->index;
        }

        if (!audio_pkt && !audio_queue_.empty()) {
            audio_queue_.pop(audio_pkt);
            av_packet_rescale_ts(audio_pkt, audio_enc_ctx_->time_base, audio_stream_->time_base);
            audio_pkt->stream_index = audio_stream_->index;
        }

        // 比较 pts 时间戳，决定先写谁
        if (video_pkt && (!audio_pkt || video_pkt->pts <= audio_pkt->pts)) {
            av_interleaved_write_frame(out_fmt_ctx_, video_pkt);
            av_packet_free(&video_pkt);
        }
        else if (audio_pkt) {
            av_interleaved_write_frame(out_fmt_ctx_, audio_pkt);
            av_packet_free(&audio_pkt);
        }
    }
   
}
