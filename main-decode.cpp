#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include "safe_queue.h"
#include <fstream>
extern "C" {

#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}



#define INBUF_SIZE 4096

static void pgm_save(unsigned char* buf, int wrap, int xsize, int ysize,
    char* filename)
{
    FILE* f;
    int i;

    fopen_s(&f, filename, "wb");
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}

static void decode(AVCodecContext* dec_ctx, AVFrame* frame, AVPacket* pkt,
    const char* filename)
{
    char buf[1024];
    int ret;

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }

        printf("saving frame %3""PRId64""\n", dec_ctx->frame_num);
        fflush(stdout);

        /* the picture is allocated by the decoder. no need to
           free it */
        snprintf(buf, sizeof(buf), "%s-%""PRId64", filename, dec_ctx->frame_num);
        pgm_save(frame->data[0], frame->linesize[0],
            frame->width, frame->height, buf);
    }
}

void write_yuv420p(std::ofstream& yuv_file, AVFrame* frame) {
    int width = frame->width;
    int height = frame->height;
    // 写Y分量
    for (int i = 0; i < height; ++i)
        yuv_file.write(reinterpret_cast<char*>(frame->data[0] + i * frame->linesize[0]), width);
    // 写U分量
    for (int i = 0; i < height / 2; ++i)
        yuv_file.write(reinterpret_cast<char*>(frame->data[1] + i * frame->linesize[1]), width / 2);
    // 写V分量
    for (int i = 0; i < height / 2; ++i)
        yuv_file.write(reinterpret_cast<char*>(frame->data[2] + i * frame->linesize[2]), width / 2);
}

int main() {
    const char* filename = "big_buck_bunny.mp4";
    AVFormatContext* fmt_ctx = nullptr;
    avformat_open_input(&fmt_ctx, filename, nullptr, nullptr);
    avformat_find_stream_info(fmt_ctx, nullptr);

    int video_stream_idx = -1;
    for (unsigned i = 0; i < fmt_ctx->nb_streams; ++i) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }
    if (video_stream_idx == -1) {
        std::cerr << "No video stream found." << std::endl;
        return -1;
    }

    const AVCodec* dec = avcodec_find_decoder(fmt_ctx->streams[video_stream_idx]->codecpar->codec_id);
    AVCodecContext* dec_ctx = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream_idx]->codecpar);
    avcodec_open2(dec_ctx, dec, nullptr);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    std::ofstream yuv_file("output.yuv", std::ios::out | std::ios::binary);
    SwsContext* sws_ctx = nullptr;
    AVPixelFormat target_fmt = AV_PIX_FMT_YUV420P;

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_stream_idx) {
            if (avcodec_send_packet(dec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(dec_ctx, frame) == 0) {
                    if (frame->format != target_fmt) {
                        // 转换为YUV420P
                        if (!sws_ctx) {
                            sws_ctx = sws_getContext(
                                frame->width, frame->height, (AVPixelFormat)frame->format,
                                frame->width, frame->height, target_fmt,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
                        }
                        AVFrame* yuv420p_frame = av_frame_alloc();
                        yuv420p_frame->format = target_fmt;
                        yuv420p_frame->width = frame->width;
                        yuv420p_frame->height = frame->height;
                        av_frame_get_buffer(yuv420p_frame, 0);
                        sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
                                  yuv420p_frame->data, yuv420p_frame->linesize);
                        write_yuv420p(yuv_file, yuv420p_frame);
                        av_frame_free(&yuv420p_frame);
                    } else {
                        write_yuv420p(yuv_file, frame);
                    }
                }
            }
        }
        av_packet_unref(pkt);
    }
    if (sws_ctx) sws_freeContext(sws_ctx);
    yuv_file.close();
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
    return 0;
}