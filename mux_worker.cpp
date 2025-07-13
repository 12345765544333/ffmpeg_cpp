#include "mux_worker.h"

MuxWorker::MuxWorker(SafeQueue<AVPacket*>& encoded_packet_queue,
                     AVFormatContext* out_fmt_ctx,
                     AVStream* out_stream,
                     AVCodecContext* video_enc_ctx,
                     std::atomic<bool>& encode_finished)
    : encoded_packet_queue_(encoded_packet_queue),
      out_fmt_ctx_(out_fmt_ctx),
      out_stream_(out_stream),
      video_enc_ctx_(video_enc_ctx),
      encode_finished_(encode_finished) {}

void MuxWorker::operator()() {
    while (!encode_finished_ || !encoded_packet_queue_.empty()) {
        AVPacket* pkt = nullptr;
        if (encoded_packet_queue_.pop(pkt)) {
            pkt->stream_index = out_stream_->index;
            av_packet_rescale_ts(pkt, video_enc_ctx_->time_base, out_stream_->time_base);
            av_interleaved_write_frame(out_fmt_ctx_, pkt);
            av_packet_free(&pkt);
        }
    }
} 