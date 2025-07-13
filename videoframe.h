#pragma once
extern "C" {
#include <libavutil/frame.h>
}

struct VideoFrame {
    AVFrame* frame;
    int64_t pts;
    int width;
    int height;
    int format;
    VideoFrame() : frame(nullptr), pts(0), width(0), height(0), format(0) {}
    VideoFrame(AVFrame* f)
        : frame(f),
          pts(f ? f->pts : 0),
          width(f ? f->width : 0),
          height(f ? f->height : 0),
          format(f ? f->format : 0) {}
}; 