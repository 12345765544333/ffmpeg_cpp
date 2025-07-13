#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <fstream>
#include "safe_queue.h"
#include "ring_buffer.h"
#include "videoframe.h"
#include "audioframe.h"
#include "demuxer.h"
#include "video_decode_worker.h"
#include "video_encode_worker.h"
#include "media_pipeline.h"
#include "audio_decode_worker.h"
#include "audio_encode_worker.h"
#include "av_mux_worker.h"

extern "C" {

#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}

int main() {
    const std::string input_file = "input.mp4";
    const std::string output_file = "./output/output4.mp4";
    MediaPipeline pipeline(input_file, output_file);
    if (!pipeline.init()) {
        return -1;
    }
    std::atomic<bool> demux_finished(false), decode_finished(false), encode_finished(false);
    std::atomic<bool> audio_decode_finished(false);
    std::atomic<bool> audio_encode_finished(false);

    // 1. Demuxer线程
    Demuxer demuxer(
        pipeline.in_fmt_ctx,
        pipeline.video_packet_queue,
        pipeline.video_stream_idx,
        &pipeline.audio_packet_queue,
        &pipeline.audio_stream_idx
    );
    std::thread demux_thread(std::ref(demuxer), &demux_finished);    

    // 2. 视频解码线程
    VideoDecodeWorker video_decode_worker(pipeline.video_packet_queue, pipeline.frame_queue, pipeline.video_dec_ctx, demux_finished, decode_finished);
    std::thread decode_thread(std::ref(video_decode_worker));

    // 2.5 音频解码线程
    std::thread audio_decode_thread;
    if (pipeline.audio_dec_ctx && pipeline.audio_stream_idx != -1) {
        audio_decode_thread = std::thread(AudioDecodeWorker(
            pipeline.audio_dec_ctx,
            pipeline.audio_stream_idx,
            pipeline.audio_packet_queue,
            pipeline.audio_frame_queue,
            demux_finished,
            audio_decode_finished
        ));
    }

    // 3. 视频编码线程
    VideoEncodeWorker video_encode_worker(pipeline.frame_queue, 
        pipeline.encoded_packet_queue, 
        pipeline.video_enc_ctx,
        decode_finished, 
        encode_finished);
    std::thread encode_thread(std::ref(video_encode_worker));

    // 3.5 音频编码线程
    std::thread audio_encode_thread;
    if (pipeline.audio_enc_ctx) {
        audio_encode_thread = std::thread(AudioEncodeWorker(
            pipeline.audio_frame_queue,
            pipeline.audio_encoded_packet_queue,
            pipeline.audio_enc_ctx,
            audio_decode_finished,
            audio_encode_finished
        ));
    }

    //// 4. 混合复用线程
    AVMuxWorker av_mux_worker(
        pipeline.encoded_packet_queue,
        pipeline.audio_encoded_packet_queue,
        pipeline.out_fmt_ctx,
        pipeline.video_stream,
        pipeline.video_enc_ctx,
        pipeline.audio_stream,
        pipeline.audio_enc_ctx,
        encode_finished,
        audio_encode_finished
    );
    std::thread mux_thread(std::ref(av_mux_worker));
   
     //等待所有线程结束
    demux_thread.join();
    if (audio_decode_thread.joinable()) audio_decode_thread.join();
    if (audio_encode_thread.joinable()) audio_encode_thread.join();
    decode_thread.join();
    encode_thread.join();
    mux_thread.join();   
    // 写入尾部
    av_write_trailer(pipeline.out_fmt_ctx);
    std::cout << "[Muxer] Muxing finished. Output: " << output_file << std::endl;
    
    return 0;
}

