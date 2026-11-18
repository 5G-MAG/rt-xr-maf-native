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

#include <chrono>
#include <thread>

#include <android/hardware_buffer.h>
#include <media/NdkImageReader.h>

extern "C"
{
#include <libavcodec/mediacodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_mediacodec.h>
#include <libavutil/pixdesc.h>
}

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
    // memory (ByteBuffer mode), or rendered to the surface of an AImageReader (surface mode), whose images are
    // AHardwareBuffers.
    class MediaCodecBackend : public VideoDecodeBackend
    {
    private:
        AVCodecContext *ctx_ = nullptr;
        AVBufferRef *device_ = nullptr;
        // shared with the images acquired from it: the reader is deleted after its last image
        std::shared_ptr<AImageReader> reader_;

        static AVPixelFormat GetSurfaceFormat(AVCodecContext *ctx, const AVPixelFormat *formats)
        {
            (void)ctx;
            for (const AVPixelFormat *f = formats; *f != AV_PIX_FMT_NONE; f++)
            {
                if (*f == AV_PIX_FMT_MEDIACODEC)
                {
                    return *f;
                }
            }
            return AV_PIX_FMT_NONE;
        }

        bool OpenSurface(AVStream *stream, int maxImages)
        {
            AImageReader *reader = nullptr;
            if (AImageReader_newWithUsage(stream->codecpar->width, stream->codecpar->height, AIMAGE_FORMAT_PRIVATE,
                                          AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, maxImages, &reader) != AMEDIA_OK)
            {
                return false;
            }
            reader_ = std::shared_ptr<AImageReader>(reader, AImageReader_delete);
            ANativeWindow *window = nullptr;
            if (AImageReader_getWindow(reader, &window) != AMEDIA_OK)
            {
                return false;
            }
            device_ = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC);
            if (!device_)
            {
                return false;
            }
            AVHWDeviceContext *device = (AVHWDeviceContext *)device_->data;
            ((AVMediaCodecDeviceContext *)device->hwctx)->native_window = window;
            if (av_hwdevice_ctx_init(device_) < 0)
            {
                return false;
            }
            ctx_->hw_device_ctx = av_buffer_ref(device_);
            ctx_->get_format = &MediaCodecBackend::GetSurfaceFormat;
            return true;
        }

    public:
        ~MediaCodecBackend() override
        {
            // the decoder stops rendering to the surface before the surface goes
            avcodec_free_context(&ctx_);
            av_buffer_unref(&device_);
        }

        bool Open(AVStream *stream, int maxImages)
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
            if (maxImages > 0 && !OpenSurface(stream, maxImages))
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
            return reader_ ? "mediacodec (surface)" : "mediacodec";
        }

        AVCodecContext *Context() override
        {
            return ctx_;
        }

        AVFrame *ToSoftware(AVFrame *decoded, AVFrame *sw) override
        {
            (void)sw;
            // a picture rendered to the surface is not in system memory
            return decoded->format == AV_PIX_FMT_MEDIACODEC ? nullptr : decoded;
        }

        NativePicture HardwarePicture(AVFrame *decoded) override
        {
            NativePicture picture;
            if (decoded->format != AV_PIX_FMT_MEDIACODEC || !reader_)
            {
                return picture;
            }
            // render the picture to the surface, then take it from the reader: pictures are rendered in order, one at a
            // time, so the next image is this picture
            if (av_mediacodec_release_buffer((AVMediaCodecBuffer *)decoded->data[3], 1) < 0)
            {
                return picture;
            }
            AImage *image = nullptr;
            auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
            media_status_t status;
            while ((status = AImageReader_acquireNextImage(reader_.get(), &image)) == AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE &&
                   std::chrono::steady_clock::now() < until)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            AHardwareBuffer *buffer = nullptr;
            if (status != AMEDIA_OK || AImage_getHardwareBuffer(image, &buffer) != AMEDIA_OK || !buffer)
            {
                if (image)
                {
                    AImage_delete(image);
                }
                return picture;
            }
            // the image returns to the reader when the frame is written again: until then, the decoder does not reuse
            // its buffer
            std::shared_ptr<AImageReader> reader = reader_;
            picture.handle = buffer;
            picture.owner = std::shared_ptr<void>(image, [reader](void *i) { AImage_delete((AImage *)i); });
            return picture;
        }
    };

} // namespace

int MediaCodecSurfaceImages(AVStream *stream, const MAF::BufferInfo &info)
{
    using namespace MAF;
    if (info.outputMedia.memory != FRAME_MEMORY_NATIVE || !info.handler)
    {
        return 0;
    }
    // the decoded pictures are handed over as they are: 4:2:0 YCbCr at the depth of the stream, at the decoded size
    int depth = 0;
    switch (info.outputMedia.format)
    {
    case G8_B8_R8_3PLANE_420_UNORM:
    case G8_B8R8_2PLANE_420_UNORM:
        depth = 8;
        break;
    case G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        depth = 10;
        break;
    default:
        return 0;
    }
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get((AVPixelFormat)stream->codecpar->format);
    if (!desc || desc->comp[0].depth != depth || desc->log2_chroma_w != 1 || desc->log2_chroma_h != 1)
    {
        return 0;
    }
    if ((info.outputMedia.width > 0 && (uint32_t)stream->codecpar->width > info.outputMedia.width) ||
        (info.outputMedia.height > 0 && (uint32_t)stream->codecpar->height > info.outputMedia.height))
    {
        return 0;
    }
    // the frames of the buffer hold one image each, and the decoder renders one more
    return info.handler->capacity() + 2;
}

std::unique_ptr<VideoDecodeBackend> OpenMediaCodecBackend(AVStream *stream, int surfaceImages)
{
    auto backend = std::make_unique<MediaCodecBackend>();
    if (!backend->Open(stream, surfaceImages))
    {
        return nullptr;
    }
    return backend;
}

#endif // __ANDROID__
