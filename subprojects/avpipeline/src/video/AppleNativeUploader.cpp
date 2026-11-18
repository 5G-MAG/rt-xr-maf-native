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

// Native pictures of Apple platforms: CVPixelBuffers backed by an IOSurface, which Metal imports without a copy

#include "NativeUploader.hpp"

#if defined(__APPLE__) && defined(MAF_NATIVE_FRAMES)

#include <algorithm>
#include <cstring>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>

using namespace MAF;

namespace
{

    // The CoreVideo pixel format of a stored format, 0 when there is none
    OSType PixelFormat(ColorFormat format, bool fullRange)
    {
        switch (format)
        {
        case BGRA:
            return kCVPixelFormatType_32BGRA;
        case RED:
        case GREEN:
        case BLUE:
        case DEPTH_COMPONENT:
            return kCVPixelFormatType_OneComponent8;
        case RG:
            return kCVPixelFormatType_TwoComponent8;
        case G8_B8_R8_3PLANE_420_UNORM:
            return fullRange ? kCVPixelFormatType_420YpCbCr8PlanarFullRange : kCVPixelFormatType_420YpCbCr8Planar;
        case G8_B8R8_2PLANE_420_UNORM:
            return fullRange ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
        case G8_B8R8_2PLANE_422_UNORM:
            return fullRange ? kCVPixelFormatType_422YpCbCr8BiPlanarFullRange : kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange;
        case G8_B8R8_2PLANE_444_UNORM:
            return fullRange ? kCVPixelFormatType_444YpCbCr8BiPlanarFullRange : kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange;
        case G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
            return fullRange ? kCVPixelFormatType_420YpCbCr10BiPlanarFullRange : kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
        case G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
            return fullRange ? kCVPixelFormatType_422YpCbCr10BiPlanarFullRange : kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange;
        case G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16:
            return fullRange ? kCVPixelFormatType_444YpCbCr10BiPlanarFullRange : kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange;
        default:
            return 0;
        }
    }

    void SetInt(CFMutableDictionaryRef dict, CFStringRef key, int value)
    {
        CFNumberRef n = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
        CFDictionarySetValue(dict, key, n);
        CFRelease(n);
    }

    class AppleNativeUploader : public NativeUploader
    {
    private:
        CVPixelBufferPoolRef pool_ = nullptr;
        VTPixelTransferSessionRef transfer_ = nullptr;
        OSType poolFormat_ = 0;
        int poolWidth_ = 0;
        int poolHeight_ = 0;

        // A pool of IOSurface-backed, Metal compatible pixel buffers of the given format and size
        bool Pool(OSType format, int width, int height)
        {
            if (pool_ && format == poolFormat_ && width == poolWidth_ && height == poolHeight_)
            {
                return true;
            }
            if (pool_)
            {
                CVPixelBufferPoolRelease(pool_);
                pool_ = nullptr;
            }
            CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFDictionaryRef surface = CFDictionaryCreate(kCFAllocatorDefault, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            SetInt(attributes, kCVPixelBufferPixelFormatTypeKey, (int)format);
            SetInt(attributes, kCVPixelBufferWidthKey, width);
            SetInt(attributes, kCVPixelBufferHeightKey, height);
            CFDictionarySetValue(attributes, kCVPixelBufferIOSurfacePropertiesKey, surface);
            CFDictionarySetValue(attributes, kCVPixelBufferMetalCompatibilityKey, kCFBooleanTrue);
            CVReturn err = CVPixelBufferPoolCreate(kCFAllocatorDefault, nullptr, attributes, &pool_);
            CFRelease(surface);
            CFRelease(attributes);
            if (err != kCVReturnSuccess)
            {
                pool_ = nullptr;
                return false;
            }
            poolFormat_ = format;
            poolWidth_ = width;
            poolHeight_ = height;
            return true;
        }

        // A new pixel buffer of the pool, or nullptr
        CVPixelBufferRef NewBuffer(OSType format, int width, int height)
        {
            CVPixelBufferRef buffer = nullptr;
            if (!Pool(format, width, height) || CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pool_, &buffer) != kCVReturnSuccess)
            {
                return nullptr;
            }
            return buffer;
        }

        static void Hold(CVPixelBufferRef buffer, NativePicture &out)
        {
            out.handle = buffer;
            out.owner = std::shared_ptr<void>(buffer, [](void *b) { CVPixelBufferRelease((CVPixelBufferRef)b); });
        }

    public:
        ~AppleNativeUploader() override
        {
            if (transfer_)
            {
                VTPixelTransferSessionInvalidate(transfer_);
                CFRelease(transfer_);
            }
            if (pool_)
            {
                CVPixelBufferPoolRelease(pool_);
            }
        }

        ColorFormat StoredFormat(ColorFormat requested) override
        {
            switch (requested)
            {
            // the color formats are stored as BGRA: sampled, a BGRA texture gives the red, green and blue components
            // (and an opaque alpha for the formats without alpha)
            case RGB:
            case RGBA:
            case BGR:
            case BGRA:
                return BGRA;
            // CoreVideo has no 3-plane formats for 4:2:2, 4:4:4 and 10 bits: the 2-plane format with the same samples
            case G8_B8_R8_3PLANE_422_UNORM:
                return G8_B8R8_2PLANE_422_UNORM;
            case G8_B8_R8_3PLANE_444_UNORM:
                return G8_B8R8_2PLANE_444_UNORM;
            case G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
                return G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
            case G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
                return G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16;
            case G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
                return G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16;
            default:
                if (PixelFormat(requested, false) == 0)
                {
                    throw "the texture format can not be stored in a native frame";
                }
                return requested;
            }
        }

        bool Upload(const PicturePlanes &planes, ColorFormat stored, int width, int height, bool fullRange, NativePicture &out) override
        {
            const OSType format = PixelFormat(stored, fullRange);
            if (format == 0)
            {
                return false;
            }
            CVPixelBufferRef buffer = NewBuffer(format, width, height);
            if (!buffer)
            {
                return false;
            }
            const bool planar = CVPixelBufferIsPlanar(buffer);
            const int count = planar ? (int)CVPixelBufferGetPlaneCount(buffer) : 1;
            if (count != planes.count || CVPixelBufferLockBaseAddress(buffer, 0) != kCVReturnSuccess)
            {
                CVPixelBufferRelease(buffer);
                return false;
            }
            for (int p = 0; p < count; p++)
            {
                uint8_t *dst = (uint8_t *)(planar ? CVPixelBufferGetBaseAddressOfPlane(buffer, p) : CVPixelBufferGetBaseAddress(buffer));
                const size_t dstStride = planar ? CVPixelBufferGetBytesPerRowOfPlane(buffer, p) : CVPixelBufferGetBytesPerRow(buffer);
                const size_t rowBytes = std::min((size_t)planes.rowBytes[p], dstStride);
                for (int y = 0; y < planes.rows[p]; y++)
                {
                    memcpy(dst + (size_t)y * dstStride, planes.data[p] + (size_t)y * (size_t)planes.stride[p], rowBytes);
                }
            }
            CVPixelBufferUnlockBaseAddress(buffer, 0);
            Hold(buffer, out);
            return true;
        }

        bool Convert(void *picture, ColorFormat stored, int width, int height, bool fullRange, NativePicture &out) override
        {
            // the formats of single components are taken from the RGB picture in system memory
            if (stored != BGRA && stored < G8_B8_R8_3PLANE_420_UNORM)
            {
                return false;
            }
            CVPixelBufferRef source = (CVPixelBufferRef)picture;
            const OSType format = PixelFormat(stored, fullRange);
            if (format == 0)
            {
                return false;
            }
            if (CVPixelBufferGetPixelFormatType(source) == format && CVPixelBufferGetWidth(source) == (size_t)width &&
                CVPixelBufferGetHeight(source) == (size_t)height && CVPixelBufferGetIOSurface(source) != nullptr)
            {
                // the decoded picture is the stored picture
                CVPixelBufferRetain(source);
                Hold(source, out);
                return true;
            }
            if (!transfer_ && VTPixelTransferSessionCreate(kCFAllocatorDefault, &transfer_) != noErr)
            {
                transfer_ = nullptr;
                return false;
            }
            CVPixelBufferRef buffer = NewBuffer(format, width, height);
            if (!buffer)
            {
                return false;
            }
            if (VTPixelTransferSessionTransferImage(transfer_, source, buffer) != noErr)
            {
                CVPixelBufferRelease(buffer);
                return false;
            }
            Hold(buffer, out);
            return true;
        }
    };

} // namespace

std::unique_ptr<NativeUploader> CreateAppleNativeUploader()
{
    return std::make_unique<AppleNativeUploader>();
}

#endif
