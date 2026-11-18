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

// The Metal importer of the maf_unity plugin, seen from Unity: the pictures of native frames (IOSurface-backed
// CVPixelBuffers) are imported as one Metal texture per plane, which read the samples of the picture.

#import <Metal/Metal.h>
#include <CoreVideo/CoreVideo.h>

#include <vector>

#include <Importers.hpp>
#include <MafUnity.h>

#include "check.hpp"

namespace {

id<MTLDevice> device()
{
    static id<MTLDevice> d = MTLCreateSystemDefaultDevice();
    return d;
}

// An IOSurface-backed pixel buffer whose sample at byte offset i of a row y of plane p is (p * 50 + y * 3 + i) & 0xFF
CVPixelBufferRef makeBuffer(OSType format, int width, int height)
{
    NSDictionary *attributes = @{
        (id)kCVPixelBufferIOSurfacePropertiesKey : @{},
        (id)kCVPixelBufferMetalCompatibilityKey : @YES,
    };
    CVPixelBufferRef buffer = nullptr;
    if (CVPixelBufferCreate(kCFAllocatorDefault, width, height, format, (__bridge CFDictionaryRef)attributes, &buffer) != kCVReturnSuccess)
        return nullptr;
    CVPixelBufferLockBaseAddress(buffer, 0);
    const bool planar = CVPixelBufferIsPlanar(buffer);
    const size_t planes = planar ? CVPixelBufferGetPlaneCount(buffer) : 1;
    for (size_t p = 0; p < planes; p++)
    {
        uint8_t *base = (uint8_t *)(planar ? CVPixelBufferGetBaseAddressOfPlane(buffer, p) : CVPixelBufferGetBaseAddress(buffer));
        size_t stride = planar ? CVPixelBufferGetBytesPerRowOfPlane(buffer, p) : CVPixelBufferGetBytesPerRow(buffer);
        size_t rows = planar ? CVPixelBufferGetHeightOfPlane(buffer, p) : CVPixelBufferGetHeight(buffer);
        for (size_t y = 0; y < rows; y++)
            for (size_t i = 0; i < stride; i++)
                base[y * stride + i] = (uint8_t)((p * 50 + y * 3 + i) & 0xFF);
    }
    CVPixelBufferUnlockBaseAddress(buffer, 0);
    return buffer;
}

int bytesPerPixel(int format)
{
    switch (format)
    {
    case MAFUNITY_FORMAT_R8: return 1;
    case MAFUNITY_FORMAT_RG8: return 2;
    case MAFUNITY_FORMAT_R16: return 2;
    case MAFUNITY_FORMAT_RG16: return 4;
    case MAFUNITY_FORMAT_BGRA8: return 4;
    default: return 0;
    }
}

struct Expected
{
    OSType format;
    int planes;
    int planeFormat[3];
    int planeWidth[3];  // for a 64x36 picture
    int planeHeight[3];
};

// Imports the pixel buffer, and checks the planes and the samples each texture reads
void checkImport(const Expected &e)
{
    std::unique_ptr<PictureImporter> importer = CreateMetalImporter(device());
    CHECK(importer != nullptr);
    if (!importer)
        return;
    CVPixelBufferRef buffer = makeBuffer(e.format, 64, 36);
    CHECK(buffer != nullptr);
    if (!buffer)
        return;
    const CFIndex refs = CFGetRetainCount(buffer);
    {
        std::shared_ptr<ImportedPicture> picture = importer->Import(buffer, ImportParams());
        CHECK(picture != nullptr);
        if (!picture)
            return;
        CHECK(CFGetRetainCount(buffer) > refs); // the import keeps the picture alive
        CHECK(picture->planeCount == e.planes);
        CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
        for (int p = 0; p < e.planes && p < picture->planeCount; p++)
        {
            CHECK(picture->format[p] == e.planeFormat[p]);
            CHECK(picture->width[p] == e.planeWidth[p] && picture->height[p] == e.planeHeight[p]);
            id<MTLTexture> texture = (__bridge id<MTLTexture>)picture->texture[p];
            CHECK(texture != nil && texture.width == (NSUInteger)e.planeWidth[p] && texture.height == (NSUInteger)e.planeHeight[p]);
            if (!texture)
                continue;
            // the texture reads the samples of the pixel buffer
            const size_t rowBytes = (size_t)e.planeWidth[p] * bytesPerPixel(e.planeFormat[p]);
            std::vector<uint8_t> read(rowBytes * e.planeHeight[p]);
            [texture getBytes:read.data() bytesPerRow:rowBytes fromRegion:MTLRegionMake2D(0, 0, e.planeWidth[p], e.planeHeight[p]) mipmapLevel:0];
            const bool planar = CVPixelBufferIsPlanar(buffer);
            const uint8_t *base = (const uint8_t *)(planar ? CVPixelBufferGetBaseAddressOfPlane(buffer, p) : CVPixelBufferGetBaseAddress(buffer));
            const size_t stride = planar ? CVPixelBufferGetBytesPerRowOfPlane(buffer, p) : CVPixelBufferGetBytesPerRow(buffer);
            bool same = true;
            for (int y = 0; y < e.planeHeight[p] && same; y++)
                same = memcmp(read.data() + y * rowBytes, base + y * stride, rowBytes) == 0;
            CHECK(same);
        }
        CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    }
    // the picture is released with the import
    CHECK(CFGetRetainCount(buffer) == refs);
    CVPixelBufferRelease(buffer);
}

} // namespace

TEST_CASE("metal: a Metal device is available")
{
    CHECK(device() != nil);
}

TEST_CASE("metal: BGRA pictures are one BGRA8 texture")
{
    checkImport({kCVPixelFormatType_32BGRA, 1, {MAFUNITY_FORMAT_BGRA8}, {64}, {36}});
}

TEST_CASE("metal: single component pictures are R8 and RG8 textures")
{
    checkImport({kCVPixelFormatType_OneComponent8, 1, {MAFUNITY_FORMAT_R8}, {64}, {36}});
    checkImport({kCVPixelFormatType_TwoComponent8, 1, {MAFUNITY_FORMAT_RG8}, {64}, {36}});
}

TEST_CASE("metal: 3-plane 4:2:0 pictures are three R8 textures")
{
    checkImport({kCVPixelFormatType_420YpCbCr8Planar, 3, {MAFUNITY_FORMAT_R8, MAFUNITY_FORMAT_R8, MAFUNITY_FORMAT_R8}, {64, 32, 32}, {36, 18, 18}});
}

TEST_CASE("metal: 2-plane 8-bit pictures are R8 and RG8 textures")
{
    checkImport({kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, 2, {MAFUNITY_FORMAT_R8, MAFUNITY_FORMAT_RG8}, {64, 32}, {36, 18}});
    checkImport({kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange, 2, {MAFUNITY_FORMAT_R8, MAFUNITY_FORMAT_RG8}, {64, 32}, {36, 36}});
    checkImport({kCVPixelFormatType_444YpCbCr8BiPlanarFullRange, 2, {MAFUNITY_FORMAT_R8, MAFUNITY_FORMAT_RG8}, {64, 64}, {36, 36}});
}

TEST_CASE("metal: 2-plane 10-bit pictures are R16 and RG16 textures")
{
    checkImport({kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, 2, {MAFUNITY_FORMAT_R16, MAFUNITY_FORMAT_RG16}, {64, 32}, {36, 18}});
    checkImport({kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange, 2, {MAFUNITY_FORMAT_R16, MAFUNITY_FORMAT_RG16}, {64, 64}, {36, 36}});
}

TEST_CASE("metal: pictures of other formats are not imported")
{
    std::unique_ptr<PictureImporter> importer = CreateMetalImporter(device());
    CVPixelBufferRef buffer = makeBuffer(kCVPixelFormatType_24RGB, 64, 36);
    if (buffer)
    {
        CHECK(importer->Import(buffer, ImportParams()) == nullptr);
        CVPixelBufferRelease(buffer);
    }
    CHECK(importer->Import(nullptr, ImportParams()) == nullptr);
}

int main()
{
    @autoreleasepool
    {
        return check::run();
    }
}
