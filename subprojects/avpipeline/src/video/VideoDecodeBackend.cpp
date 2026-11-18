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

#include <VideoDecodeBackend.hpp>

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

std::unique_ptr<VideoDecodeBackend> CreateVideoBackend(AVStream *stream, const MAF::BufferInfo &info)
{
    (void)info;
    return std::make_unique<SoftwareBackend>(OpenSoftwareDecoder(stream));
}
