#include "audio_mux_worker.h"
#include<iostream>
AudioMuxWorker::AudioMuxWorker(SafeQueue<AVPacket*>& encoded_packet_queue,
                               AVFormatContext* out_fmt_ctx,
                               AVStream* out_stream,
                               AVCodecContext* enc_ctx,
                               std::atomic<bool>& encode_finished)
    : encoded_packet_queue_(encoded_packet_queue),
      out_fmt_ctx_(out_fmt_ctx),
      out_stream_(out_stream),
      enc_ctx_(enc_ctx),
      encode_finished_(encode_finished) {}

void AudioMuxWorker::operator()() {
    std::cout << "[AudioMuxWorker] Start audio muxing..." << std::endl;
    while (!encode_finished_ || !encoded_packet_queue_.empty()) {
        AVPacket* pkt = nullptr;
        if (encoded_packet_queue_.pop(pkt)) {
            pkt->stream_index = out_stream_->index;
            av_packet_rescale_ts(pkt, enc_ctx_->time_base, out_stream_->time_base);
            std::cout << "[AudioMuxWorker] Rescaled packet pts: " << pkt->pts << ", dts: " << pkt->dts << std::endl;
            int ret = av_interleaved_write_frame(out_fmt_ctx_, pkt);
            if (ret < 0) {
                std::cerr << "[AudioMuxWorker] Error writing frame. FFmpeg error code: " << ret<<std::endl;
                av_packet_free(&pkt);
                break; // 防止阻塞
            }
            av_packet_free(&pkt);
        }
    }
}
