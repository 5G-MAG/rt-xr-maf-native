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

#include <cstdlib>
#include <cstring>

#include "Backends.hpp"

namespace
{

    // The libav software decoder: pictures are decoded in system memory
    class SoftwareBackend : public VideoDecodeBackend
    {
    private:
        AVCodecContext *ctx_;

    public:
        explicit SoftwareBackend(AVCodecContext *ctx) : ctx_(ctx) {}
        ~SoftwareBackend() override
        {
            avcodec_free_context(&ctx_);
        }
        const char *Name() const override
        {
            return "software";
        }
        AVCodecContext *Context() override
        {
            return ctx_;
        }
    };

} // namespace

AVCodecContext *OpenSoftwareDecoder(AVStream *stream)
{
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec)
    {
        throw AVERROR(EINVAL);
    }
    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx)
    {
        throw AVERROR(ENOMEM);
    }
    int err = avcodec_parameters_to_context(ctx, stream->codecpar);
    if (err >= 0)
    {
        err = avcodec_open2(ctx, codec, nullptr);
    }
    if (err < 0)
    {
        avcodec_free_context(&ctx);
        throw err;
    }
    return ctx;
}

namespace
{

    // The environment variable AVPIPELINE_HWDECODE changes the use of hardware decoders at run time: "0" uses the
    // software decoders only, "require" fails when no hardware decoder can decode the stream (for tests)
    enum class HwDecode
    {
        AUTO,
        OFF,
        REQUIRED
    };

    HwDecode hwDecodeMode()
    {
        const char *v = std::getenv("AVPIPELINE_HWDECODE");
        if (v && std::strcmp(v, "0") == 0)
        {
            return HwDecode::OFF;
        }
        if (v && std::strcmp(v, "require") == 0)
        {
            return HwDecode::REQUIRED;
        }
        return HwDecode::AUTO;
    }

} // namespace

bool HardwareRequired()
{
    return hwDecodeMode() == HwDecode::REQUIRED;
}

std::unique_ptr<VideoDecodeBackend> CreateVideoBackend(AVStream *stream, const MAF::BufferInfo &info)
{
    (void)info;
    const HwDecode mode = hwDecodeMode();
#if AVPIPELINE_HWDECODE
    if (mode != HwDecode::OFF)
    {
        std::unique_ptr<VideoDecodeBackend> backend;
#if defined(__APPLE__)
        backend = OpenHwAccelBackend(stream, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, mode == HwDecode::REQUIRED);
#elif defined(__ANDROID__)
        backend = OpenMediaCodecBackend(stream);
#endif
        if (backend)
        {
            return backend;
        }
    }
#endif
    if (mode == HwDecode::REQUIRED)
    {
        throw "no hardware decoder for the stream";
    }
    return std::make_unique<SoftwareBackend>(OpenSoftwareDecoder(stream));
}
