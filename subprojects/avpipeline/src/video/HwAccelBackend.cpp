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

extern "C"
{
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

namespace
{

    class HwAccelBackend : public VideoDecodeBackend
    {
    private:
        AVCodecContext *ctx_ = nullptr;
        AVBufferRef *device_ = nullptr;
        AVHWDeviceType type_;
        AVPixelFormat hwFormat_;
        bool required_;

        // Picks the hardware format; when the hardware can not decode the stream, libav calls it again without the
        // hardware format, and the software format the decoder prefers is used, unless the hardware is required
        static AVPixelFormat GetFormat(AVCodecContext *ctx, const AVPixelFormat *formats)
        {
            const HwAccelBackend *self = static_cast<const HwAccelBackend *>(ctx->opaque);
            for (const AVPixelFormat *f = formats; *f != AV_PIX_FMT_NONE; f++)
            {
                if (*f == self->hwFormat_)
                {
                    return *f;
                }
            }
            if (self->required_)
            {
                return AV_PIX_FMT_NONE;
            }
            for (const AVPixelFormat *f = formats; *f != AV_PIX_FMT_NONE; f++)
            {
                const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*f);
                if (desc && !(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
                {
                    return *f;
                }
            }
            return AV_PIX_FMT_NONE;
        }

    public:
        HwAccelBackend(AVHWDeviceType type, AVPixelFormat hwFormat, bool required) : type_(type), hwFormat_(hwFormat), required_(required) {}

        ~HwAccelBackend() override
        {
            avcodec_free_context(&ctx_);
            av_buffer_unref(&device_);
        }

        bool Open(AVStream *stream)
        {
            const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
            if (!codec || av_hwdevice_ctx_create(&device_, type_, nullptr, nullptr, 0) < 0)
            {
                return false;
            }
            ctx_ = avcodec_alloc_context3(codec);
            if (!ctx_ || avcodec_parameters_to_context(ctx_, stream->codecpar) < 0)
            {
                return false;
            }
            ctx_->hw_device_ctx = av_buffer_ref(device_);
            ctx_->opaque = this;
            ctx_->get_format = &HwAccelBackend::GetFormat;
            return avcodec_open2(ctx_, codec, nullptr) >= 0;
        }

        const char *Name() const override
        {
            return av_hwdevice_get_type_name(type_);
        }

        AVCodecContext *Context() override
        {
            return ctx_;
        }

        AVFrame *ToSoftware(AVFrame *decoded, AVFrame *sw) override
        {
            if (decoded->format != hwFormat_)
            {
                return decoded;
            }
            av_frame_unref(sw);
            if (av_hwframe_transfer_data(sw, decoded, 0) < 0 || av_frame_copy_props(sw, decoded) < 0)
            {
                return nullptr;
            }
            return sw;
        }
    };

    // The pixel format of the pictures the codec decodes with a device of the given type, or AV_PIX_FMT_NONE
    AVPixelFormat HwFormat(const AVCodec *codec, AVHWDeviceType type)
    {
        for (int i = 0;; i++)
        {
            const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
            if (!config)
            {
                return AV_PIX_FMT_NONE;
            }
            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && config->device_type == type)
            {
                return config->pix_fmt;
            }
        }
    }

} // namespace

std::unique_ptr<VideoDecodeBackend> OpenHwAccelBackend(AVStream *stream, AVHWDeviceType type, bool required)
{
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec)
    {
        return nullptr;
    }
    AVPixelFormat hwFormat = HwFormat(codec, type);
    if (hwFormat == AV_PIX_FMT_NONE)
    {
        return nullptr;
    }
    auto backend = std::make_unique<HwAccelBackend>(type, hwFormat, required);
    if (!backend->Open(stream))
    {
        return nullptr;
    }
    return backend;
}
