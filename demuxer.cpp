#include "demuxer.h"
#include <iostream>

Demuxer::Demuxer(AVFormatContext* fmt_ctx,
                 SafeQueue<AVPacket*>& video_queue, int& video_stream_idx,
                 SafeQueue<AVPacket*>* audio_queue, int* audio_stream_idx)
    : fmt_ctx_(fmt_ctx), video_queue_(video_queue), video_stream_idx_(video_stream_idx),
      audio_queue_(audio_queue), audio_stream_idx_(audio_stream_idx) {}

Demuxer::~Demuxer() {
    // 不再负责fmt_ctx_的释放，由外部管理
}

void Demuxer::demux(std::atomic<bool>* finished_flag) {
    AVPacket* pkt = av_packet_alloc();
    while (av_read_frame(fmt_ctx_, pkt) >= 0) {
        if (pkt->stream_index == video_stream_idx_) {
            video_queue_.push(av_packet_clone(pkt));
        } else if (audio_queue_ && audio_stream_idx_ && pkt->stream_index == *audio_stream_idx_) {
            audio_queue_->push(av_packet_clone(pkt));
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    if (finished_flag) *finished_flag = true;
}

void Demuxer::operator()(std::atomic<bool>* finished_flag) {
    demux(finished_flag);
}
