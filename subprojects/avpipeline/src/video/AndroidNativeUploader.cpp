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

// Native pictures of Android: AHardwareBuffers, which Vulkan and GLES import without a copy

#include "NativeUploader.hpp"

#if defined(__ANDROID__) && defined(MAF_NATIVE_FRAMES)

#include <cstring>

#include <android/hardware_buffer.h>

using namespace MAF;

namespace
{

    // the pictures are written by the CPU, sampled by the GPU, and may be read back by the CPU (tests)
    const uint64_t kUsage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN | AHARDWAREBUFFER_USAGE_CPU_READ_RARELY;

    // The AHardwareBuffer format of a stored format, 0 when there is none
    uint32_t BufferFormat(ColorFormat format)
    {
        switch (format)
        {
        case RGBA:
            return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
        case RED:
        case GREEN:
        case BLUE:
        case DEPTH_COMPONENT:
            return AHARDWAREBUFFER_FORMAT_R8_UNORM;
        case G8_B8_R8_3PLANE_420_UNORM:
        case G8_B8R8_2PLANE_420_UNORM:
            return AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;
        case G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        case G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
            return AHARDWAREBUFFER_FORMAT_YCbCr_P010;
        default:
            return 0;
        }
    }

    bool IsTenBits(ColorFormat format)
    {
        return format >= G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16;
    }

    // Writes rows of samples of bytesPerSample bytes, pixelStride bytes apart in the destination
    void CopySamples(uint8_t *dst, int dstRowStride, int pixelStride, const uint8_t *src, int srcStride, int samples, int rows, int bytesPerSample, int srcStep)
    {
        for (int y = 0; y < rows; y++)
        {
            uint8_t *d = dst + (size_t)y * (size_t)dstRowStride;
            const uint8_t *s = src + (size_t)y * (size_t)srcStride;
            if (pixelStride == bytesPerSample && srcStep == bytesPerSample)
            {
                memcpy(d, s, (size_t)samples * (size_t)bytesPerSample);
                continue;
            }
            for (int x = 0; x < samples; x++)
            {
                memcpy(d + (size_t)x * (size_t)pixelStride, s + (size_t)x * (size_t)srcStep, (size_t)bytesPerSample);
            }
        }
    }

    class AndroidNativeUploader : public NativeUploader
    {
    public:
        ColorFormat StoredFormat(ColorFormat requested) override
        {
            ColorFormat stored = requested;
            switch (requested)
            {
            // the color formats are stored as RGBA: sampled, an RGBA texture gives the red, green and blue components
            // (and an opaque alpha for the formats without alpha)
            case RGB:
            case RGBA:
            case BGR:
            case BGRA:
                stored = RGBA;
                break;
            default:
                break;
            }
            const uint32_t format = BufferFormat(stored);
            if (format == 0)
            {
                // e.g. YCbCr 4:2:2 and 4:4:4, which have no AHardwareBuffer format
                throw "the texture format can not be stored in a native frame on Android";
            }
            if (__builtin_available(android 29, *))
            {
                AHardwareBuffer_Desc desc = {};
                desc.width = 64;
                desc.height = 64;
                desc.layers = 1;
                desc.format = format;
                desc.usage = kUsage;
                if (!AHardwareBuffer_isSupported(&desc))
                {
                    throw "the device can not store the texture format in a native frame";
                }
                return stored;
            }
            throw "native frame buffers require Android 10 (API 29)";
        }

        bool Upload(const PicturePlanes &planes, ColorFormat stored, int width, int height, bool fullRange, NativePicture &out) override
        {
            (void)fullRange; // the range is given by MPEG_sampler_YCbCr, an AHardwareBuffer has none
            if (__builtin_available(android 29, *))
            {
                AHardwareBuffer_Desc desc = {};
                desc.width = (uint32_t)width;
                desc.height = (uint32_t)height;
                desc.layers = 1;
                desc.format = BufferFormat(stored);
                desc.usage = kUsage;
                AHardwareBuffer *buffer = nullptr;
                if (desc.format == 0 || AHardwareBuffer_allocate(&desc, &buffer) != 0)
                {
                    return false;
                }
                bool ok = stored >= G8_B8_R8_3PLANE_420_UNORM ? WriteYCbCr(buffer, planes, stored, width, height) : WritePacked(buffer, planes);
                if (!ok)
                {
                    AHardwareBuffer_release(buffer);
                    return false;
                }
                out.handle = buffer;
                out.owner = std::shared_ptr<void>(buffer, [](void *b) { AHardwareBuffer_release((AHardwareBuffer *)b); });
                return true;
            }
            return false;
        }

        bool Convert(const NativePicture &picture, ColorFormat stored, int width, int height, bool fullRange, NativePicture &out) override
        {
            (void)fullRange;
            // the pictures of MediaCodec are handed over: 4:2:0 YCbCr, possibly larger than the frame (aligned sizes)
            if (stored < G8_B8_R8_3PLANE_420_UNORM || BufferFormat(stored) == 0)
            {
                return false;
            }
            AHardwareBuffer_Desc desc = {};
            AHardwareBuffer_describe((AHardwareBuffer *)picture.handle, &desc);
            if (desc.width < (uint32_t)width || desc.height < (uint32_t)height)
            {
                return false;
            }
            // a format of the device (e.g. IMPLEMENTATION_DEFINED) has the depth of the stream, which MediaCodec chose
            if ((desc.format == AHARDWAREBUFFER_FORMAT_YCbCr_P010 && !IsTenBits(stored)) ||
                (desc.format == AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420 && IsTenBits(stored)))
            {
                return false;
            }
            out = picture;
            return true;
        }

    private:
        static bool WritePacked(AHardwareBuffer *buffer, const PicturePlanes &planes) __attribute__((availability(android, introduced = 29)))
        {
            AHardwareBuffer_Desc desc = {};
            AHardwareBuffer_describe(buffer, &desc);
            void *data = nullptr;
            if (planes.count != 1 || AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1, nullptr, &data) != 0)
            {
                return false;
            }
            const int bytesPerPixel = desc.format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM ? 4 : 1;
            // the stride of an AHardwareBuffer is in pixels
            CopySamples((uint8_t *)data, (int)desc.stride * bytesPerPixel, planes.rowBytes[0], planes.data[0], planes.stride[0], 1, planes.rows[0],
                        planes.rowBytes[0], planes.rowBytes[0]);
            AHardwareBuffer_unlock(buffer, nullptr);
            return true;
        }

        // The vendor lays out the planes of a YCbCr buffer: lockPlanes gives their row and pixel strides
        static bool WriteYCbCr(AHardwareBuffer *buffer, const PicturePlanes &planes, ColorFormat stored, int width, int height) __attribute__((availability(android, introduced = 29)))
        {
            AHardwareBuffer_Planes dst = {};
            if (AHardwareBuffer_lockPlanes(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1, nullptr, &dst) != 0)
            {
                return false;
            }
            if (dst.planeCount != 3)
            {
                AHardwareBuffer_unlock(buffer, nullptr);
                return false;
            }
            const int bps = IsTenBits(stored) ? 2 : 1;
            const int cw = (width + 1) / 2;
            const int ch = (height + 1) / 2;
            CopySamples((uint8_t *)dst.planes[0].data, (int)dst.planes[0].rowStride, (int)dst.planes[0].pixelStride, planes.data[0], planes.stride[0],
                        width, height, bps, bps);
            for (int c = 0; c < 2; c++)
            {
                // the source chroma: one plane per component (3 planes), or interleaved components (2 planes)
                const bool interleaved = planes.count == 2;
                const uint8_t *src = interleaved ? planes.data[1] + c * bps : planes.data[1 + c];
                const int srcStride = interleaved ? planes.stride[1] : planes.stride[1 + c];
                CopySamples((uint8_t *)dst.planes[1 + c].data, (int)dst.planes[1 + c].rowStride, (int)dst.planes[1 + c].pixelStride, src, srcStride, cw,
                            ch, bps, interleaved ? 2 * bps : bps);
            }
            AHardwareBuffer_unlock(buffer, nullptr);
            return true;
        }
    };

} // namespace

std::unique_ptr<NativeUploader> CreateAndroidNativeUploader()
{
    return std::make_unique<AndroidNativeUploader>();
}

#endif
