/*
 * Copyright (c) 2023 MotionSpell
 * Licensed under the License terms and conditions for use, reproduction,
 * and distribution of 5GMAG software (the “License”).
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at https://www.5g-mag.com/license .
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and limitations under the License.
 */

// Test media: video clips with a known pattern, encoded at test time with the libav libraries the pipeline uses.

#pragma once

#include <string>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace testmedia {

// Value of the sample at (x, y) of a plane (0: Y, 1: Cb, 2: Cr) in frame k, for samples of depth bits
inline int sample(int plane, int x, int y, int k, int depth)
{
    return (x * 3 + y * 5 + plane * 40 + k * 7) & ((1 << depth) - 1);
}

struct Clip
{
    std::string path;
    AVPixelFormat format; // planar format the pattern is drawn in
    int width;
    int height;
    int frames;
    int fps;
    AVPixelFormat encodeFormat = AV_PIX_FMT_NONE; // format given to the encoder, when it is not format
};

// Encodes clip.frames frames of the pattern in clip.format with the given encoder (lossless FFV1 by default), in a
// container chosen from the extension of clip.path, with the given bit rate and distance between intra pictures (the
// encoder defaults, one second, when 0). Returns an empty string, or the error.
inline std::string write(const Clip &clip, const char *encoder = "ffv1", int bitRate = 0, int gopSize = 0)
{
    const AVPixelFormat encodeFormat = clip.encodeFormat != AV_PIX_FMT_NONE ? clip.encodeFormat : clip.format;
    SwsContext *sws = nullptr;
    AVFrame *encoded = nullptr;
    const AVCodec *codec = avcodec_find_encoder_by_name(encoder);
    if (!codec)
        return std::string("encoder not available: ") + encoder;
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(clip.format);
    if (!desc)
        return "unknown pixel format";
    const int depth = desc->comp[0].depth;
    const int bytesPerSample = depth > 8 ? 2 : 1;

    AVFormatContext *oc = nullptr;
    AVCodecContext *c = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *pkt = nullptr;
    std::string error;

    auto writePackets = [&]() -> int {
        int err;
        while ((err = avcodec_receive_packet(c, pkt)) >= 0)
        {
            av_packet_rescale_ts(pkt, c->time_base, oc->streams[0]->time_base);
            pkt->stream_index = 0;
            if ((err = av_interleaved_write_frame(oc, pkt)) < 0)
                return err;
        }
        return (err == AVERROR(EAGAIN) || err == AVERROR_EOF) ? 0 : err;
    };

    do
    {
        if (avformat_alloc_output_context2(&oc, nullptr, nullptr, clip.path.c_str()) < 0 || !oc)
        {
            error = "no container for " + clip.path;
            break;
        }
        AVStream *st = avformat_new_stream(oc, nullptr);
        c = avcodec_alloc_context3(codec);
        if (!st || !c)
        {
            error = "out of memory";
            break;
        }
        c->width = clip.width;
        c->height = clip.height;
        c->pix_fmt = encodeFormat;
        if (bitRate > 0)
            c->bit_rate = bitRate;
        c->time_base = {1, clip.fps};
        c->framerate = {clip.fps, 1};
        c->gop_size = gopSize > 0 ? gopSize : clip.fps;
        if (oc->oformat->flags & AVFMT_GLOBALHEADER)
            c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        if (avcodec_open2(c, codec, nullptr) < 0)
        {
            error = std::string("can not open the encoder for ") + desc->name;
            break;
        }
        avcodec_parameters_from_context(st->codecpar, c);
        st->time_base = c->time_base;
        if (avio_open(&oc->pb, clip.path.c_str(), AVIO_FLAG_WRITE) < 0 || avformat_write_header(oc, nullptr) < 0)
        {
            error = "can not write " + clip.path;
            break;
        }

        frame = av_frame_alloc();
        pkt = av_packet_alloc();
        frame->format = clip.format;
        frame->width = clip.width;
        frame->height = clip.height;
        if (av_frame_get_buffer(frame, 0) < 0)
        {
            error = "out of memory";
            break;
        }
        if (encodeFormat != clip.format)
        {
            encoded = av_frame_alloc();
            encoded->format = encodeFormat;
            encoded->width = clip.width;
            encoded->height = clip.height;
            sws = sws_getContext(clip.width, clip.height, clip.format, clip.width, clip.height, encodeFormat, SWS_POINT, nullptr, nullptr, nullptr);
            if (av_frame_get_buffer(encoded, 0) < 0 || !sws)
            {
                error = "can not convert to the encoder format";
                break;
            }
        }
        for (int k = 0; k < clip.frames && error.empty(); k++)
        {
            av_frame_make_writable(frame);
            for (int p = 0; p < desc->nb_components && p < 3; p++)
            {
                const int pw = p == 0 ? clip.width : AV_CEIL_RSHIFT(clip.width, desc->log2_chroma_w);
                const int ph = p == 0 ? clip.height : AV_CEIL_RSHIFT(clip.height, desc->log2_chroma_h);
                for (int y = 0; y < ph; y++)
                {
                    uint8_t *row = frame->data[p] + (ptrdiff_t)y * frame->linesize[p];
                    for (int x = 0; x < pw; x++)
                    {
                        int v = sample(p, x, y, k, depth);
                        if (bytesPerSample == 2)
                        {
                            row[2 * x] = (uint8_t)(v & 0xFF);
                            row[2 * x + 1] = (uint8_t)(v >> 8);
                        }
                        else
                        {
                            row[x] = (uint8_t)v;
                        }
                    }
                }
            }
            frame->pts = k;
            AVFrame *input = frame;
            if (encoded)
            {
                av_frame_make_writable(encoded);
                sws_scale(sws, frame->data, frame->linesize, 0, clip.height, encoded->data, encoded->linesize);
                encoded->pts = k;
                input = encoded;
            }
            if (avcodec_send_frame(c, input) < 0 || writePackets() < 0)
                error = "encoding failed";
        }
        if (!error.empty())
            break;
        if (avcodec_send_frame(c, nullptr) < 0 || writePackets() < 0 || av_write_trailer(oc) < 0)
            error = "encoding failed";
    } while (false);

    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_frame_free(&encoded);
    sws_freeContext(sws);
    avcodec_free_context(&c);
    if (oc)
    {
        if (oc->pb)
            avio_closep(&oc->pb);
        avformat_free_context(oc);
    }
    return error;
}

} // namespace testmedia
