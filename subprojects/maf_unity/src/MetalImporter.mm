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

// Imports the CVPixelBuffers of native frames as Metal textures, one per plane, through a CVMetalTextureCache: the
// textures use the IOSurface of the pixel buffer, nothing is copied.

#include "Importers.hpp"

#include <CoreVideo/CoreVideo.h>

#include "MafUnity.h"

namespace
{

    struct PlaneLayout
    {
        int count = 0;
        MTLPixelFormat pixelFormat[3] = {MTLPixelFormatInvalid, MTLPixelFormatInvalid, MTLPixelFormatInvalid};
        int format[3] = {MAFUNITY_FORMAT_NONE, MAFUNITY_FORMAT_NONE, MAFUNITY_FORMAT_NONE};
    };

    PlaneLayout Planes(int count, MTLPixelFormat luma, int lumaFormat, MTLPixelFormat chroma = MTLPixelFormatInvalid, int chromaFormat = MAFUNITY_FORMAT_NONE)
    {
        PlaneLayout l;
        l.count = count;
        l.pixelFormat[0] = luma;
        l.format[0] = lumaFormat;
        for (int p = 1; p < count; p++)
        {
            l.pixelFormat[p] = chroma;
            l.format[p] = chromaFormat;
        }
        return l;
    }

    // The planes of the pixel formats of native frames (see docs/maf.md)
    PlaneLayout LayoutOf(OSType format)
    {
        switch (format)
        {
        case kCVPixelFormatType_32BGRA:
            return Planes(1, MTLPixelFormatBGRA8Unorm, MAFUNITY_FORMAT_BGRA8);
        case kCVPixelFormatType_OneComponent8:
            return Planes(1, MTLPixelFormatR8Unorm, MAFUNITY_FORMAT_R8);
        case kCVPixelFormatType_TwoComponent8:
            return Planes(1, MTLPixelFormatRG8Unorm, MAFUNITY_FORMAT_RG8);
        case kCVPixelFormatType_420YpCbCr8Planar:
        case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
            return Planes(3, MTLPixelFormatR8Unorm, MAFUNITY_FORMAT_R8, MTLPixelFormatR8Unorm, MAFUNITY_FORMAT_R8);
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
        case kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange:
        case kCVPixelFormatType_422YpCbCr8BiPlanarFullRange:
        case kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange:
        case kCVPixelFormatType_444YpCbCr8BiPlanarFullRange:
            return Planes(2, MTLPixelFormatR8Unorm, MAFUNITY_FORMAT_R8, MTLPixelFormatRG8Unorm, MAFUNITY_FORMAT_RG8);
        case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
        case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange:
        case kCVPixelFormatType_422YpCbCr10BiPlanarFullRange:
        case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
        case kCVPixelFormatType_444YpCbCr10BiPlanarFullRange:
            return Planes(2, MTLPixelFormatR16Unorm, MAFUNITY_FORMAT_R16, MTLPixelFormatRG16Unorm, MAFUNITY_FORMAT_RG16);
        default:
            return PlaneLayout();
        }
    }

    class MetalImporter : public PictureImporter
    {
    private:
        CVMetalTextureCacheRef cache_ = nullptr;

    public:
        explicit MetalImporter(id<MTLDevice> device)
        {
            if (CVMetalTextureCacheCreate(kCFAllocatorDefault, nullptr, device, nullptr, &cache_) != kCVReturnSuccess)
            {
                cache_ = nullptr;
            }
        }

        ~MetalImporter() override
        {
            if (cache_)
            {
                CFRelease(cache_);
            }
        }

        bool Valid() const
        {
            return cache_ != nullptr;
        }

        std::shared_ptr<ImportedPicture> Import(void *picture) override
        {
            CVPixelBufferRef buffer = (CVPixelBufferRef)picture;
            if (!buffer || CFGetTypeID(buffer) != CVPixelBufferGetTypeID())
            {
                return nullptr;
            }
            const PlaneLayout layout = LayoutOf(CVPixelBufferGetPixelFormatType(buffer));
            const bool planar = CVPixelBufferIsPlanar(buffer);
            if (layout.count == 0 || (planar ? (int)CVPixelBufferGetPlaneCount(buffer) : 1) != layout.count)
            {
                return nullptr;
            }
            // the textures of the pictures no longer used
            CVMetalTextureCacheFlush(cache_, 0);

            CVMetalTextureRef textures[3] = {nullptr, nullptr, nullptr};
            auto imported = std::make_shared<ImportedPicture>();
            imported->planeCount = layout.count;
            bool ok = true;
            for (int p = 0; p < layout.count && ok; p++)
            {
                const size_t w = planar ? CVPixelBufferGetWidthOfPlane(buffer, p) : CVPixelBufferGetWidth(buffer);
                const size_t h = planar ? CVPixelBufferGetHeightOfPlane(buffer, p) : CVPixelBufferGetHeight(buffer);
                ok = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, cache_, buffer, nullptr, layout.pixelFormat[p], w, h,
                                                               planar ? p : 0, &textures[p]) == kCVReturnSuccess &&
                     textures[p] != nullptr;
                if (ok)
                {
                    imported->texture[p] = (__bridge void *)CVMetalTextureGetTexture(textures[p]);
                    imported->format[p] = layout.format[p];
                    imported->width[p] = (int)w;
                    imported->height[p] = (int)h;
                }
            }
            CVPixelBufferRetain(buffer);
            // the textures, then the pixel buffer: what uses the IOSurface is released first
            imported->release = [textures, buffer]() {
                for (CVMetalTextureRef t : textures)
                {
                    if (t)
                    {
                        CFRelease(t);
                    }
                }
                CVPixelBufferRelease(buffer);
            };
            return ok ? imported : nullptr;
        }
    };

} // namespace

std::unique_ptr<PictureImporter> CreateMetalImporter(id<MTLDevice> device)
{
    if (!device)
    {
        return nullptr;
    }
    auto importer = std::make_unique<MetalImporter>(device);
    if (!importer->Valid())
    {
        return nullptr;
    }
    return importer;
}
