#pragma once
#include "safe_queue.h"
#include "ring_buffer.h"
#include "videoframe.h"
#include "audioframe.h"
#include "demuxer.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}
#include <string>

class MediaPipeline {
public:
    MediaPipeline(const std::string& in_filename, const std::string& out_filename);
    ~MediaPipeline();

    bool init(); // 一步完成所有初始化

    // 资源
    AVFormatContext* in_fmt_ctx;
    AVCodecContext* video_dec_ctx;
    AVCodecContext* video_enc_ctx;
    AVCodecContext* audio_dec_ctx;
    AVCodecContext* audio_enc_ctx;
    AVFormatContext* out_fmt_ctx;
    AVStream* video_stream;
    AVStream* audio_stream;
    int video_stream_idx;
    int audio_stream_idx;
    AVRational input_fps;

    // 队列
    SafeQueue<AVPacket*> video_packet_queue;
    SafeQueue<AVPacket*> audio_packet_queue;
    RingBuffer<VideoFrame> frame_queue;
    RingBuffer<AudioFrame> audio_frame_queue{100};
    SafeQueue<AVPacket*> encoded_packet_queue;
    SafeQueue<AVPacket*> audio_encoded_packet_queue;
private:
    std::string in_filename_;
    std::string out_filename_;
}; 