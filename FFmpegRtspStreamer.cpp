#include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdlib>
#include <cstdio>

// FFmpeg headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

using namespace cv;
using namespace std;

int main(int argc, char** argv) {
    // Initialize FFmpeg
    avformat_network_init();

    // Open webcam
    VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "Error: Could not open camera" << endl;
        return -1;
    }

    // Get frame size
    int width = static_cast<int>(cap.get(CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(cap.get(CAP_PROP_FRAME_HEIGHT));
    int fps = static_cast<int>(cap.get(CAP_PROP_FPS));
    if (fps == 0) fps = 30; // default fps if CAP_PROP_FPS is not available

    // FFmpeg variables
    AVFormatContext* fmt_ctx = nullptr;
    AVStream* video_st = nullptr;
    AVCodecContext* c = nullptr;
    const AVCodec* codec = nullptr; // Use const AVCodec*
    AVFrame* frame = nullptr;
    AVPacket pkt;
    struct SwsContext* sws_ctx = nullptr;

    // Output URL
    const char* out_url = "rtsp://127.0.0.1:8554/camera";

    // Allocate format context
    avformat_alloc_output_context2(&fmt_ctx, nullptr, "rtsp", out_url);
    if (!fmt_ctx) {
        cerr << "Could not allocate format context" << endl;
        return -1;
    }

    // Find the H.264 video encoder
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        cerr << "Codec not found" << endl;
        return -1;
    }

    // Add a new stream to the format context
    video_st = avformat_new_stream(fmt_ctx, codec);
    if (!video_st) {
        cerr << "Could not allocate stream" << endl;
        return -1;
    }

    // Allocate codec context
    c = avcodec_alloc_context3(codec);
    if (!c) {
        cerr << "Could not allocate video codec context" << endl;
        return -1;
    }

    // Set codec parameters
    c->codec_id = codec->id;
    c->bit_rate = 400000;
    c->width = width;
    c->height = height;
    c->time_base = av_make_q(1, fps);  // Use av_make_q to set AVRational
    c->framerate = av_make_q(fps, 1);  // Use av_make_q to set AVRational
    c->gop_size = 12;
    c->max_b_frames = 2;
    c->pix_fmt = AV_PIX_FMT_YUV420P;

    // Open codec
    if (avcodec_open2(c, codec, nullptr) < 0) {
        cerr << "Could not open codec" << endl;
        return -1;
    }

    // Set stream parameters
    avcodec_parameters_from_context(video_st->codecpar, c);
    video_st->time_base = c->time_base;

    // Open output URL
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx->pb, out_url, AVIO_FLAG_WRITE) < 0) {
            cerr << "Could not open output URL" << endl;
            return -1;
        }
    }

    // Write header
    if (avformat_write_header(fmt_ctx, nullptr) < 0) {
        cerr << "Error occurred when opening output URL" << endl;
        return -1;
    }

    // Allocate frame
    frame = av_frame_alloc();
    if (!frame) {
        cerr << "Could not allocate video frame" << endl;
        return -1;
    }
    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;

    // Allocate the buffers for the frame data
    if (av_frame_get_buffer(frame, 32) < 0) {
        cerr << "Could not allocate frame data buffers" << endl;
        return -1;
    }

    // Allocate SWScale context
    sws_ctx = sws_getContext(width, height, AV_PIX_FMT_BGR24, width, height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx) {
        cerr << "Could not initialize the conversion context" << endl;
        return -1;
    }

    // Capture loop
    int64_t pts = 0;
    while (true) {
        Mat img;
        cap >> img;
        if (img.empty()) {
            cerr << "Error: Could not grab frame" << endl;
            break;
        }

        // Convert OpenCV frame (BGR) to YUV
        const int stride[] = { static_cast<int>(img.step[0]) };
        sws_scale(sws_ctx, &img.data, stride, 0, height, frame->data, frame->linesize);

        frame->pts = pts++;

        // Encode frame
        av_init_packet(&pkt);
        pkt.data = nullptr;
        pkt.size = 0;
        if (avcodec_send_frame(c, frame) < 0) {
            cerr << "Error sending frame to codec context" << endl;
            break;
        }

        // Receive packet
        if (avcodec_receive_packet(c, &pkt) == 0) {
            pkt.stream_index = video_st->index;
            av_packet_rescale_ts(&pkt, c->time_base, video_st->time_base);
            av_interleaved_write_frame(fmt_ctx, &pkt);
            av_packet_unref(&pkt);
        }

        // Show frame (optional)
        imshow("Webcam", img);
        if (waitKey(1) >= 0) break;
    }

    // Flush encoder
    avcodec_send_frame(c, nullptr);
    while (avcodec_receive_packet(c, &pkt) == 0) {
        pkt.stream_index = video_st->index;
        av_packet_rescale_ts(&pkt, c->time_base, video_st->time_base);
        av_interleaved_write_frame(fmt_ctx, &pkt);
        av_packet_unref(&pkt);
    }

    // Write trailer
    av_write_trailer(fmt_ctx);

    // Cleanup
    av_frame_free(&frame);
    avcodec_free_context(&c);
    avio_closep(&fmt_ctx->pb);
    avformat_free_context(fmt_ctx);
    cap.release();
    sws_freeContext(sws_ctx);

    return 0;
}
