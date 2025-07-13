#include "media_pipeline.h"
#include <iostream>

MediaPipeline::MediaPipeline(const std::string& in_filename, const std::string& out_filename)
    : in_fmt_ctx(nullptr), video_dec_ctx(nullptr), video_enc_ctx(nullptr),
      out_fmt_ctx(nullptr), video_stream(nullptr), audio_stream(nullptr),
      video_stream_idx(-1), audio_stream_idx(-1),
      frame_queue(100), // 可自定义大小
      in_filename_(in_filename), out_filename_(out_filename)
{
}

MediaPipeline::~MediaPipeline() {
    if (video_dec_ctx) avcodec_free_context(&video_dec_ctx);
    if (video_enc_ctx) avcodec_free_context(&video_enc_ctx);
    if (out_fmt_ctx) {
        if (out_fmt_ctx->pb) avio_close(out_fmt_ctx->pb);
        avformat_free_context(out_fmt_ctx);
    }
    if (in_fmt_ctx) {
        avformat_close_input(&in_fmt_ctx);
    }
}

bool MediaPipeline::init() {
    // 1. 打开输入文件并查找流信息
    if (avformat_open_input(&in_fmt_ctx, in_filename_.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Failed to open input file: " << in_filename_ << std::endl;
        return false;
    }
    if (avformat_find_stream_info(in_fmt_ctx, nullptr) < 0) {
        std::cerr << "Failed to get stream info." << std::endl;
        return false;
    }
    video_stream_idx = -1;
    audio_stream_idx = -1;
    for (unsigned i = 0; i < in_fmt_ctx->nb_streams; ++i) {
        if (in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1) {
            video_stream_idx = i;
        }
        if (in_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx == -1) {
            audio_stream_idx = i;
        }
    }
    if (video_stream_idx == -1) {
        std::cerr << "No video stream found." << std::endl;
        return false;
    }

    // 2. 初始化视频解码器
    AVFormatContext* fmt_ctx = in_fmt_ctx;
    int v_idx = video_stream_idx;
    const AVCodecParameters* vpar = fmt_ctx->streams[v_idx]->codecpar;
    const AVCodec* dec = avcodec_find_decoder(vpar->codec_id);
    video_dec_ctx = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(video_dec_ctx, vpar);
    avcodec_open2(video_dec_ctx, dec, nullptr);
    //AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_CUDA;
    //AVBufferRef* hw_device_ctx = nullptr;

    //// step 1: 查找支持硬件加速的解码器
    //const AVCodec* dec = avcodec_find_decoder_by_name("h264_qsv");  // 或 "hevc_cuvid" 等
    //if (!dec) {
    //    std::cerr << "Failed to find CUDA decoder, fallback to software decoder." << std::endl;
    //    dec = avcodec_find_decoder(vpar->codec_id);
    //}

    //// step 2: 创建解码上下文
    //video_dec_ctx = avcodec_alloc_context3(dec);
    //avcodec_parameters_to_context(video_dec_ctx, vpar);
    //// step 3: 硬件上下文准备
    //if (av_hwdevice_ctx_create(&hw_device_ctx, hw_type, nullptr, nullptr, 0) < 0) {
    //    std::cerr << "Failed to create HW device context, fallback to software decoding." << std::endl;
    //    hw_device_ctx = nullptr;  // fallback
    //}
    //else {
    //    video_dec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    //    video_dec_ctx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
    //        for (const enum AVPixelFormat* p = pix_fmts; *p != -1; p++) {
    //            if (*p == AV_PIX_FMT_CUDA) {
    //                return *p;
    //            }
    //        }
    //        std::cerr << "CUDA format not found, fallback to " << av_get_pix_fmt_name(pix_fmts[0]) << std::endl;
    //        return pix_fmts[0];
    //    };
    //}
    //// step 4: 打开解码器
    //if (avcodec_open2(video_dec_ctx, dec, nullptr) < 0) {
    //    std::cerr << "Failed to open decoder.\n";
    //    return false;
    //}

    // 2.5 初始化音频解码器
    audio_dec_ctx = nullptr;
    if (audio_stream_idx != -1) {
        int a_idx = audio_stream_idx;
        const AVCodecParameters* apar = fmt_ctx->streams[a_idx]->codecpar;
        const AVCodec* audio_dec = avcodec_find_decoder(apar->codec_id);
        audio_dec_ctx = avcodec_alloc_context3(audio_dec);
        avcodec_parameters_to_context(audio_dec_ctx, apar);
        avcodec_open2(audio_dec_ctx, audio_dec, nullptr);
    }
    

    // 3. 初始化视频编码器
    int width = video_dec_ctx->width;
    int height = video_dec_ctx->height;
    AVRational input_fps = fmt_ctx->streams[v_idx]->avg_frame_rate;
    if (input_fps.num == 0 || input_fps.den == 0) input_fps = {25, 1};
    this->input_fps = input_fps;
    int fps_num = input_fps.num;
    int fps_den = input_fps.den;
    const AVCodec* video_enc = avcodec_find_encoder(AV_CODEC_ID_H264);
    video_enc_ctx = avcodec_alloc_context3(video_enc);
    video_enc_ctx->bit_rate = 800 * 1000;
    video_enc_ctx->width = width;
    video_enc_ctx->height = height;
    video_enc_ctx->time_base = AVRational{ fps_den, fps_num };
    video_enc_ctx->framerate = AVRational{ fps_num, fps_den };
    video_enc_ctx->gop_size = 12;
    video_enc_ctx->max_b_frames = 2;
    video_enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "preset", "medium", 0);
    avcodec_open2(video_enc_ctx, video_enc, &opts);
    av_dict_free(&opts);

    // 3.5 初始化音频编码器
    audio_enc_ctx = nullptr;
    if (audio_dec_ctx) {
        const AVCodec* audio_enc = avcodec_find_encoder(AV_CODEC_ID_AAC); // AAC格式
        audio_enc_ctx = avcodec_alloc_context3(audio_enc);
        audio_enc_ctx->sample_rate = 22050;
        av_channel_layout_default(&audio_enc_ctx->ch_layout, 2);
        audio_enc_ctx->sample_fmt = audio_enc->sample_fmts[0];
        audio_enc_ctx->bit_rate = 65000;
        audio_enc_ctx->time_base = { 1, audio_enc_ctx->sample_rate };
        avcodec_open2(audio_enc_ctx, audio_enc, nullptr);
    }

    // 4. 初始化输出文件
    avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, out_filename_.c_str());
    // 视频流
    video_stream = avformat_new_stream(out_fmt_ctx, nullptr);
    avcodec_parameters_from_context(video_stream->codecpar, video_enc_ctx);
    video_stream->codecpar->codec_tag = 0;
    video_stream->time_base = video_enc_ctx->time_base;
    // 音频流
    audio_stream = nullptr;
    if (audio_enc_ctx) {
        audio_stream = avformat_new_stream(out_fmt_ctx, nullptr);
        avcodec_parameters_from_context(audio_stream->codecpar, audio_enc_ctx);
        audio_stream->time_base = audio_enc_ctx->time_base;
    }
    // 打开文件
    if (avio_open(&out_fmt_ctx->pb, out_filename_.c_str(), AVIO_FLAG_WRITE) < 0) {
        std::cerr << "Could not open output file." << std::endl;
        return false;
    }
    // 写文件头
    if (avformat_write_header(out_fmt_ctx, nullptr) < 0) {
        std::cerr << "Error occurred when opening output file" << std::endl;
        return false;
    }
    return true;
} 