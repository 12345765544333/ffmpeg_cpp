#pragma once
extern "C" {
#include <libavutil/frame.h>
}

struct AudioFrame {
    AVFrame* frame;
    int64_t pts;
    int nb_samples;
    int channels;
    int format;
    AudioFrame() : frame(nullptr), pts(0), nb_samples(0), channels(0), format(0) {}
    AudioFrame(AVFrame* f)
        : frame(f),
          pts(f ? f->pts : 0),
          nb_samples(f ? f->nb_samples : 0),
          channels(f ? f->ch_layout.nb_channels : 0),
          format(f ? f->format : 0) {}
}; 