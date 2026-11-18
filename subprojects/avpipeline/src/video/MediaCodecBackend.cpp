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

#include "Backends.hpp"

#ifdef __ANDROID__

namespace
{

    // The MediaCodec decoders of libav for a codec, or nullptr
    const char *MediaCodecDecoderName(AVCodecID id)
    {
        switch (id)
        {
        case AV_CODEC_ID_H264:
            return "h264_mediacodec";
        case AV_CODEC_ID_HEVC:
            return "hevc_mediacodec";
        case AV_CODEC_ID_VP8:
            return "vp8_mediacodec";
        case AV_CODEC_ID_VP9:
            return "vp9_mediacodec";
        case AV_CODEC_ID_AV1:
            return "av1_mediacodec";
        case AV_CODEC_ID_MPEG4:
            return "mpeg4_mediacodec";
        case AV_CODEC_ID_MPEG2VIDEO:
            return "mpeg2_mediacodec";
        default:
            return nullptr;
        }
    }

    // A MediaCodec decoder, through the NDK API (no Java VM needed). The pictures are output in buffers of system
    // memory (ByteBuffer mode).
    class MediaCodecBackend : public VideoDecodeBackend
    {
    private:
        AVCodecContext *ctx_ = nullptr;

    public:
        ~MediaCodecBackend() override
        {
            avcodec_free_context(&ctx_);
        }

        bool Open(AVStream *stream)
        {
            const char *name = MediaCodecDecoderName(stream->codecpar->codec_id);
            const AVCodec *codec = name ? avcodec_find_decoder_by_name(name) : nullptr;
            if (!codec)
            {
                return false;
            }
            ctx_ = avcodec_alloc_context3(codec);
            if (!ctx_ || avcodec_parameters_to_context(ctx_, stream->codecpar) < 0)
            {
                return false;
            }
            AVDictionary *options = nullptr;
            av_dict_set(&options, "ndk_codec", "1", 0);
            int err = avcodec_open2(ctx_, codec, &options);
            av_dict_free(&options);
            return err >= 0;
        }

        const char *Name() const override
        {
            return "mediacodec";
        }

        AVCodecContext *Context() override
        {
            return ctx_;
        }
    };

} // namespace

std::unique_ptr<VideoDecodeBackend> OpenMediaCodecBackend(AVStream *stream)
{
    auto backend = std::make_unique<MediaCodecBackend>();
    if (!backend->Open(stream))
    {
        return nullptr;
    }
    return backend;
}

#endif // __ANDROID__
