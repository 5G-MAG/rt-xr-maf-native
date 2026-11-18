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

// The avpipeline media pipeline seen from the consumer (the Presentation Engine): the test encodes a clip with a
// known pattern, plays it through AvMediaPipeline into a texture buffer, and checks every sample of the frames read.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <vector>
#include <algorithm>
#include <set>
#include <thread>

#include <AvPipeline.hpp>

#if defined(__APPLE__)
#include <CoreVideo/CoreVideo.h>
#elif defined(__ANDROID__)
#include <android/hardware_buffer.h>
#endif

#include "check.hpp"
#include "test_media.hpp"

using namespace MAF;

namespace {

// Layout of a YCbCr ColorFormat, as documented in maf.hpp
struct Layout
{
    int sx, sy;     // chroma subsampling
    int depth;      // bits per sample
    bool twoPlanes; // Y, then interleaved CbCr
};

Layout layoutOf(ColorFormat f)
{
    switch (f)
    {
    case G8_B8_R8_3PLANE_420_UNORM: return {2, 2, 8, false};
    case G8_B8R8_2PLANE_420_UNORM: return {2, 2, 8, true};
    case G8_B8_R8_3PLANE_422_UNORM: return {2, 1, 8, false};
    case G8_B8R8_2PLANE_422_UNORM: return {2, 1, 8, true};
    case G8_B8_R8_3PLANE_444_UNORM: return {1, 1, 8, false};
    case G8_B8R8_2PLANE_444_UNORM: return {1, 1, 8, true};
    case G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return {2, 2, 10, false};
    case G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return {2, 2, 10, true};
    case G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return {2, 1, 10, false};
    case G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return {2, 1, 10, true};
    case G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return {1, 1, 10, false};
    case G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16: return {1, 1, 10, true};
    default: return {0, 0, 0, false};
    }
}

size_t storedBytes(const Layout &l, int w, int h)
{
    int bps = l.depth > 8 ? 2 : 1;
    int cw = (w + l.sx - 1) / l.sx, ch = (h + l.sy - 1) / l.sy;
    return ((size_t)w * h + 2 * (size_t)cw * ch) * bps;
}

// Largest difference between the stored samples of picture k and the pattern; -1 when the layout is wrong
// (a 10-bit sample with low bits set: G10X6 holds the value in the 10 most significant bits)
int maxError(const uint8_t *data, const Layout &l, int w, int h, int k)
{
    const int bps = l.depth > 8 ? 2 : 1;
    const int cw = (w + l.sx - 1) / l.sx, ch = (h + l.sy - 1) / l.sy;
    int worst = 0;
    auto at = [&](size_t sampleIndex) -> int {
        if (bps == 1)
            return data[sampleIndex];
        int word = data[2 * sampleIndex] | (data[2 * sampleIndex + 1] << 8);
        if (word & 0x3F)
            return -1;
        return word >> 6;
    };
    auto compare = [&](int got, int plane, int x, int y) {
        if (got < 0)
        {
            worst = -1;
            return;
        }
        int d = std::abs(got - testmedia::sample(plane, x, y, k, l.depth));
        if (worst >= 0 && d > worst)
            worst = d;
    };
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            compare(at((size_t)y * w + x), 0, x, y);
    const size_t chroma = (size_t)w * h;
    for (int y = 0; y < ch; y++)
        for (int x = 0; x < cw; x++)
        {
            if (l.twoPlanes)
            {
                compare(at(chroma + 2 * ((size_t)y * cw + x)), 1, x, y);
                compare(at(chroma + 2 * ((size_t)y * cw + x) + 1), 2, x, y);
            }
            else
            {
                compare(at(chroma + (size_t)y * cw + x), 1, x, y);
                compare(at(chroma + (size_t)cw * ch + (size_t)y * cw + x), 2, x, y);
            }
        }
    return worst;
}

std::string clipPath(const char *name)
{
    return (std::filesystem::temp_directory_path() / (std::string("avpipeline_test_") + name + ".mkv")).string();
}

enum class Memory
{
    CPU,
    NATIVE
};

// Plays the clip into a texture buffer of the given format and maximum size, and calls onFrame with each frame read
// and the index of its picture in the clip. Frames of a native buffer are released once onFrame returns. Returns the
// number of pictures seen.
int play(const testmedia::Clip &clip, ColorFormat format, int maxWidth, int maxHeight, const std::function<void(Frame *, int)> &onFrame,
         Memory memory = Memory::CPU)
{
    const Layout layout = layoutOf(format);
    BufferHandler handler;
    if (memory == Memory::NATIVE)
        handler.allocateNative(4);
    else
        handler.allocate(4, (uint32_t)storedBytes(layout, maxWidth, maxHeight));

    BufferInfo info{};
    info.bufferId = 0;
    info.handler = &handler;
    info.componentType = layout.depth > 8 ? UNSIGNED_SHORT : UNSIGNED_BYTE;
    info.type = SCALAR;
    info.offset = 0;
    info.stride = 0;
    info.attributeType = ATTRIB_COLOR;
    info.outputMedia.width = (uint32_t)maxWidth;
    info.outputMedia.height = (uint32_t)maxHeight;
    info.outputMedia.format = format;
    info.outputMedia.memory = memory == Memory::NATIVE ? FRAME_MEMORY_NATIVE : FRAME_MEMORY_CPU;

    AlternativeLocation alt;
    alt.mimeType = "video/x-matroska";
    alt.uri = clip.path;
    MediaInfo media;
    media.name = "test";
    media.alternatives.push_back(alt);

    AvMediaPipeline pipeline;
    pipeline.initialize(media, {info});
    CHECK(pipeline.state() == READY);
    if (pipeline.state() != READY)
        return 0;
    TimeInfo time{0.0, 0.0, true, false};
    ViewInfo view{};
    pipeline.startFetching(time, view);

    std::set<int> seen;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while ((int)seen.size() < clip.frames && std::chrono::steady_clock::now() < deadline)
    {
        Frame *f = handler.readFrame();
        if (!f)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        int k = (int)std::lround(getTime_s(f->timestamp) * clip.fps);
        onFrame(f, k);
        seen.insert(k);
        if (memory == Memory::NATIVE)
            handler.releaseFrame(f->index);
    }
    CHECK((int)seen.size() == clip.frames);
    pipeline.stopFetching();
    return (int)seen.size();
}

// Writes the clip, or fails the test case
bool writeClip(const testmedia::Clip &clip)
{
    std::string err = testmedia::write(clip);
    if (!err.empty())
        std::fprintf(stderr, "  %s\n", err.c_str());
    CHECK(err.empty());
    return err.empty();
}

std::string caseFileName(const char *caseName)
{
    std::string name = caseName;
    for (char &ch : name)
        if (ch == ' ')
            ch = '_';
    return clipPath(name.c_str());
}

struct FormatCase
{
    const char *name;
    AVPixelFormat source;
    ColorFormat stored;
    int width, height;
};

const FormatCase kFormatCases[] = {
    {"yuv420p to 3-plane 420", AV_PIX_FMT_YUV420P, G8_B8_R8_3PLANE_420_UNORM, 64, 36},
    {"yuv420p to 2-plane 420", AV_PIX_FMT_YUV420P, G8_B8R8_2PLANE_420_UNORM, 64, 36},
    {"yuv420p odd size to 3-plane 420", AV_PIX_FMT_YUV420P, G8_B8_R8_3PLANE_420_UNORM, 33, 17},
    {"yuv422p to 3-plane 422", AV_PIX_FMT_YUV422P, G8_B8_R8_3PLANE_422_UNORM, 64, 36},
    {"yuv422p to 2-plane 422", AV_PIX_FMT_YUV422P, G8_B8R8_2PLANE_422_UNORM, 64, 36},
    {"yuv444p to 3-plane 444", AV_PIX_FMT_YUV444P, G8_B8_R8_3PLANE_444_UNORM, 64, 36},
    {"yuv444p to 2-plane 444", AV_PIX_FMT_YUV444P, G8_B8R8_2PLANE_444_UNORM, 64, 36},
    {"yuv420p10 to 10-bit 3-plane 420", AV_PIX_FMT_YUV420P10LE, G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16, 64, 36},
    {"yuv420p10 to 10-bit 2-plane 420", AV_PIX_FMT_YUV420P10LE, G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, 64, 36},
    {"yuv422p10 to 10-bit 3-plane 422", AV_PIX_FMT_YUV422P10LE, G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16, 64, 36},
    {"yuv422p10 to 10-bit 2-plane 422", AV_PIX_FMT_YUV422P10LE, G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16, 64, 36},
    {"yuv444p10 to 10-bit 3-plane 444", AV_PIX_FMT_YUV444P10LE, G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16, 64, 36},
    {"yuv444p10 to 10-bit 2-plane 444", AV_PIX_FMT_YUV444P10LE, G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16, 64, 36},
};

const bool registeredFormatCases = [] {
    for (const FormatCase &fc : kFormatCases)
    {
        check::cases().push_back({fc.name, [fc] {
            testmedia::Clip clip{caseFileName(fc.name), fc.source, fc.width, fc.height, 10, 25};
            if (!writeClip(clip))
                return;
            // the source has the sampling and depth of the stored format: the samples are stored exactly
            const Layout layout = layoutOf(fc.stored);
            play(clip, fc.stored, fc.width, fc.height, [&](Frame *f, int k) {
                CHECK(f->length == storedBytes(layout, clip.width, clip.height));
                CHECK((f->extraFrameInfoFlags & 0x01) && f->width == (uint32_t)clip.width && f->height == (uint32_t)clip.height);
                int err = maxError(f->data, layout, clip.width, clip.height, k);
                if (err != 0)
                    std::fprintf(stderr, "  frame %d: max sample error %d\n", k, err);
                CHECK(err == 0);
            });
            std::filesystem::remove(clip.path);
        }});
    }
    return true;
}();

// Largest difference between the stored luma of picture k and the pattern of a source of srcDepth bits
int lumaError(const uint8_t *data, const Layout &l, int w, int h, int k, int srcDepth)
{
    int worst = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            size_t i = (size_t)y * w + x;
            int got = l.depth > 8 ? ((data[2 * i] | (data[2 * i + 1] << 8)) >> 6) : data[i];
            int expected = testmedia::sample(0, x, y, k, srcDepth);
            // a sample of srcDepth bits stored with depth bits: full scale maps to full scale
            expected = (int)std::lround(expected * (double)((1 << l.depth) - 1) / ((1 << srcDepth) - 1));
            worst = std::max(worst, std::abs(got - expected));
        }
    return worst;
}

TEST_CASE("conversion: 4:2:0 source stored as 4:4:4, the luma is stored exactly")
{
    testmedia::Clip clip{caseFileName("420_to_444"), AV_PIX_FMT_YUV420P, 64, 36, 5, 25};
    if (!writeClip(clip))
        return;
    const Layout layout = layoutOf(G8_B8R8_2PLANE_444_UNORM);
    play(clip, G8_B8R8_2PLANE_444_UNORM, 64, 36, [&](Frame *f, int k) {
        CHECK(f->length == storedBytes(layout, 64, 36));
        CHECK(lumaError(f->data, layout, 64, 36, k, 8) == 0);
    });
    std::filesystem::remove(clip.path);
}

TEST_CASE("conversion: 8-bit source stored as 10-bit")
{
    testmedia::Clip clip{caseFileName("8_to_10_bit"), AV_PIX_FMT_YUV420P, 64, 36, 5, 25};
    if (!writeClip(clip))
        return;
    const Layout layout = layoutOf(G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16);
    play(clip, G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, 64, 36, [&](Frame *f, int k) {
        CHECK(f->length == storedBytes(layout, 64, 36));
        int err = lumaError(f->data, layout, 64, 36, k, 8);
        if (err > 4)
            std::fprintf(stderr, "  frame %d: max luma error %d\n", k, err);
        CHECK(err <= 4);
    });
    std::filesystem::remove(clip.path);
}

TEST_CASE("conversion: a picture larger than the texture is reduced, its size is in the frame")
{
    testmedia::Clip clip{caseFileName("downscale"), AV_PIX_FMT_YUV420P, 64, 36, 5, 25};
    if (!writeClip(clip))
        return;
    const Layout layout = layoutOf(G8_B8_R8_3PLANE_420_UNORM);
    play(clip, G8_B8_R8_3PLANE_420_UNORM, 32, 18, [&](Frame *f, int) {
        CHECK((f->extraFrameInfoFlags & 0x01) && f->width == 32 && f->height == 18);
        CHECK(f->length == storedBytes(layout, 32, 18));
    });
    std::filesystem::remove(clip.path);
}

#if defined(__APPLE__) && defined(MAF_NATIVE_FRAMES)

// The format a native picture of a requested YCbCr format is stored in on Apple platforms: CoreVideo has no 3-plane
// formats for 4:2:2, 4:4:4 and 10 bits, the 2-plane format has the same samples
ColorFormat appleStoredFormat(ColorFormat requested)
{
    switch (requested)
    {
    case G8_B8_R8_3PLANE_422_UNORM: return G8_B8R8_2PLANE_422_UNORM;
    case G8_B8_R8_3PLANE_444_UNORM: return G8_B8R8_2PLANE_444_UNORM;
    case G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
    case G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16;
    case G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16;
    default: return requested;
    }
}

// The CoreVideo pixel format of a stored YCbCr format, video range
OSType applePixelFormat(ColorFormat stored)
{
    switch (stored)
    {
    case G8_B8_R8_3PLANE_420_UNORM: return kCVPixelFormatType_420YpCbCr8Planar;
    case G8_B8R8_2PLANE_420_UNORM: return kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    case G8_B8R8_2PLANE_422_UNORM: return kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange;
    case G8_B8R8_2PLANE_444_UNORM: return kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange;
    case G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
    case G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange;
    case G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16: return kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange;
    default: return 0;
    }
}

// The samples of a planar pixel buffer, without the padding of its rows, in the layout of maf.hpp
std::vector<uint8_t> readPlanes(CVPixelBufferRef buffer, const Layout &l)
{
    const int bps = l.depth > 8 ? 2 : 1;
    std::vector<uint8_t> out;
    CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    for (size_t p = 0; p < CVPixelBufferGetPlaneCount(buffer); p++)
    {
        const uint8_t *base = (const uint8_t *)CVPixelBufferGetBaseAddressOfPlane(buffer, p);
        const size_t stride = CVPixelBufferGetBytesPerRowOfPlane(buffer, p);
        const size_t rowBytes = CVPixelBufferGetWidthOfPlane(buffer, p) * bps * ((p > 0 && l.twoPlanes) ? 2 : 1);
        for (size_t y = 0; y < CVPixelBufferGetHeightOfPlane(buffer, p); y++)
            out.insert(out.end(), base + y * stride, base + y * stride + rowBytes);
    }
    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    return out;
}

const bool registeredNativeFormatCases = [] {
    for (const FormatCase &fc : kFormatCases)
    {
        std::string name = std::string("native buffer: ") + fc.name;
        check::cases().push_back({strdup(name.c_str()), [fc] {
            testmedia::Clip clip{caseFileName((std::string("native_") + fc.name).c_str()), fc.source, fc.width, fc.height, 10, 25};
            if (!writeClip(clip))
                return;
            const ColorFormat stored = appleStoredFormat(fc.stored);
            const Layout layout = layoutOf(stored);
            play(clip, fc.stored, fc.width, fc.height, [&](Frame *f, int k) {
                CHECK(f->length == 0);
                CHECK((f->extraFrameInfoFlags & 0x01) && f->width == (uint32_t)clip.width && f->height == (uint32_t)clip.height);
                CVPixelBufferRef buffer = (CVPixelBufferRef)f->data;
                CHECK(buffer != nullptr);
                if (!buffer)
                    return;
                CHECK(CVPixelBufferGetPixelFormatType(buffer) == applePixelFormat(stored));
                CHECK(CVPixelBufferGetIOSurface(buffer) != nullptr);
                CHECK(CVPixelBufferGetWidth(buffer) == (size_t)clip.width && CVPixelBufferGetHeight(buffer) == (size_t)clip.height);
                std::vector<uint8_t> samples = readPlanes(buffer, layout);
                CHECK(samples.size() == storedBytes(layout, clip.width, clip.height));
                if (samples.size() != storedBytes(layout, clip.width, clip.height))
                    return;
                int err = maxError(samples.data(), layout, clip.width, clip.height, k);
                if (err != 0)
                    std::fprintf(stderr, "  frame %d: max sample error %d\n", k, err);
                CHECK(err == 0);
            }, Memory::NATIVE);
            std::filesystem::remove(clip.path);
        }});
    }
    return true;
}();

TEST_CASE("native buffer: RGB and RGBA textures are BGRA pixel buffers")
{
    testmedia::Clip clip{caseFileName("native_rgba"), AV_PIX_FMT_YUV420P, 64, 36, 3, 25};
    if (!writeClip(clip))
        return;
    for (ColorFormat format : {RGB, RGBA, BGRA})
    {
        play(clip, format, 64, 36, [&](Frame *f, int) {
            CVPixelBufferRef buffer = (CVPixelBufferRef)f->data;
            CHECK(buffer && CVPixelBufferGetPixelFormatType(buffer) == kCVPixelFormatType_32BGRA);
            if (!buffer)
                return;
            CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
            const uint8_t *px = (const uint8_t *)CVPixelBufferGetBaseAddress(buffer);
            CHECK(px[3] == 255); // opaque
            CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
        }, Memory::NATIVE);
    }
    std::filesystem::remove(clip.path);
}

#endif // __APPLE__ && MAF_NATIVE_FRAMES

#if defined(__ANDROID__) && defined(MAF_NATIVE_FRAMES)

// Whether Android stores the requested YCbCr format natively: 4:2:0 only, AHardwareBuffer has no 4:2:2 or 4:4:4
bool androidStoresNatively(ColorFormat requested)
{
    switch (requested)
    {
    case G8_B8_R8_3PLANE_420_UNORM:
    case G8_B8R8_2PLANE_420_UNORM:
    case G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
    case G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        return true;
    default:
        return false;
    }
}

// The samples of a YCbCr AHardwareBuffer, without the padding of its rows, in the layout of maf.hpp
std::vector<uint8_t> readPlanes(AHardwareBuffer *buffer, const Layout &l, int w, int h)
{
    std::vector<uint8_t> out;
    if (__builtin_available(android 29, *))
    {
        AHardwareBuffer_Planes planes = {};
        if (AHardwareBuffer_lockPlanes(buffer, AHARDWAREBUFFER_USAGE_CPU_READ_RARELY, -1, nullptr, &planes) != 0 || planes.planeCount != 3)
            return out;
        const int bps = l.depth > 8 ? 2 : 1;
        const int cw = (w + 1) / 2, ch = (h + 1) / 2;
        auto sampleAt = [&](int p, int x, int y) {
            const uint8_t *s = (const uint8_t *)planes.planes[p].data + (size_t)y * planes.planes[p].rowStride + (size_t)x * planes.planes[p].pixelStride;
            return std::vector<uint8_t>(s, s + bps);
        };
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                auto v = sampleAt(0, x, y);
                out.insert(out.end(), v.begin(), v.end());
            }
        if (l.twoPlanes)
        {
            for (int y = 0; y < ch; y++)
                for (int x = 0; x < cw; x++)
                    for (int c = 1; c <= 2; c++)
                    {
                        auto v = sampleAt(c, x, y);
                        out.insert(out.end(), v.begin(), v.end());
                    }
        }
        else
        {
            for (int c = 1; c <= 2; c++)
                for (int y = 0; y < ch; y++)
                    for (int x = 0; x < cw; x++)
                    {
                        auto v = sampleAt(c, x, y);
                        out.insert(out.end(), v.begin(), v.end());
                    }
        }
        AHardwareBuffer_unlock(buffer, nullptr);
    }
    return out;
}

const bool registeredAndroidNativeFormatCases = [] {
    for (const FormatCase &fc : kFormatCases)
    {
        std::string name = std::string("native buffer: ") + fc.name;
        check::cases().push_back({strdup(name.c_str()), [fc] {
            testmedia::Clip clip{caseFileName((std::string("native_") + fc.name).c_str()), fc.source, fc.width, fc.height, 10, 25};
            if (!writeClip(clip))
                return;
            const Layout layout = layoutOf(fc.stored);
            if (!androidStoresNatively(fc.stored))
            {
                // initialize() rejects the buffer: the Presentation Engine uses a CPU buffer
                BufferHandler handler;
                handler.allocateNative(4);
                BufferInfo info{};
                info.handler = &handler;
                info.componentType = layout.depth > 8 ? UNSIGNED_SHORT : UNSIGNED_BYTE;
                info.type = SCALAR;
                info.outputMedia.width = (uint32_t)fc.width;
                info.outputMedia.height = (uint32_t)fc.height;
                info.outputMedia.format = fc.stored;
                info.outputMedia.memory = FRAME_MEMORY_NATIVE;
                AlternativeLocation alt;
                alt.uri = clip.path;
                MediaInfo media;
                media.alternatives.push_back(alt);
                AvMediaPipeline pipeline;
                pipeline.initialize(media, {info});
                CHECK(pipeline.state() == ERROR);
                std::filesystem::remove(clip.path);
                return;
            }
            play(clip, fc.stored, fc.width, fc.height, [&](Frame *f, int k) {
                CHECK(f->length == 0);
                CHECK((f->extraFrameInfoFlags & 0x01) && f->width == (uint32_t)clip.width && f->height == (uint32_t)clip.height);
                AHardwareBuffer *buffer = (AHardwareBuffer *)f->data;
                CHECK(buffer != nullptr);
                if (!buffer)
                    return;
                AHardwareBuffer_Desc desc = {};
                AHardwareBuffer_describe(buffer, &desc);
                CHECK(desc.width == (uint32_t)clip.width && desc.height == (uint32_t)clip.height);
                CHECK(desc.format == (layout.depth > 8 ? (uint32_t)AHARDWAREBUFFER_FORMAT_YCbCr_P010 : (uint32_t)AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420));
                std::vector<uint8_t> samples = readPlanes(buffer, layout, clip.width, clip.height);
                CHECK(samples.size() == storedBytes(layout, clip.width, clip.height));
                if (samples.size() != storedBytes(layout, clip.width, clip.height))
                    return;
                int err = maxError(samples.data(), layout, clip.width, clip.height, k);
                if (err != 0)
                    std::fprintf(stderr, "  frame %d: max sample error %d\n", k, err);
                CHECK(err == 0);
            }, Memory::NATIVE);
            std::filesystem::remove(clip.path);
        }});
    }
    return true;
}();

TEST_CASE("native buffer: RGB and RGBA textures are RGBA hardware buffers")
{
    testmedia::Clip clip{caseFileName("native_rgba"), AV_PIX_FMT_YUV420P, 64, 36, 3, 25};
    if (!writeClip(clip))
        return;
    for (ColorFormat format : {RGB, RGBA, BGRA})
    {
        play(clip, format, 64, 36, [&](Frame *f, int) {
            AHardwareBuffer *buffer = (AHardwareBuffer *)f->data;
            CHECK(buffer != nullptr);
            if (!buffer)
                return;
            AHardwareBuffer_Desc desc = {};
            AHardwareBuffer_describe(buffer, &desc);
            CHECK(desc.format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);
            void *px = nullptr;
            if (AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_READ_RARELY, -1, nullptr, &px) == 0)
            {
                CHECK(((const uint8_t *)px)[3] == 255); // opaque
                AHardwareBuffer_unlock(buffer, nullptr);
            }
        }, Memory::NATIVE);
    }
    std::filesystem::remove(clip.path);
}

#endif // __ANDROID__ && MAF_NATIVE_FRAMES

#ifdef MAF_NATIVE_FRAMES

TEST_CASE("native buffer: frames that are not released stop the producer, releasing them resumes it")
{
    testmedia::Clip clip{caseFileName("native_hold"), AV_PIX_FMT_YUV420P, 64, 36, 10, 25};
    if (!writeClip(clip))
        return;
    BufferHandler handler;
    handler.allocateNative(4);
    BufferInfo info{};
    info.handler = &handler;
    info.componentType = UNSIGNED_BYTE;
    info.type = SCALAR;
    info.outputMedia.width = 64;
    info.outputMedia.height = 36;
    info.outputMedia.format = G8_B8R8_2PLANE_420_UNORM;
    info.outputMedia.memory = FRAME_MEMORY_NATIVE;
    AlternativeLocation alt;
    alt.uri = clip.path;
    MediaInfo media;
    media.alternatives.push_back(alt);
    AvMediaPipeline pipeline;
    pipeline.initialize(media, {info});
    CHECK(pipeline.state() == READY);
    pipeline.startFetching(TimeInfo{0.0, 0.0, true, false}, ViewInfo{});

    // read everything that arrives within half a second, without releasing anything
    std::vector<Frame *> held;
    std::set<void *> pictures;
    auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (std::chrono::steady_clock::now() < until)
    {
        if (Frame *f = handler.readFrame())
        {
            held.push_back(f);
            pictures.insert(f->data);
        }
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    // the producer can not write a held frame: at most one picture per frame of the buffer
    CHECK(held.size() == handler.capacity());
    CHECK(pictures.size() == held.size());

    for (Frame *f : held)
        handler.releaseFrame(f->index);
    int more = 0;
    until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (more < clip.frames - (int)held.size() && std::chrono::steady_clock::now() < until)
    {
        if (Frame *f = handler.readFrame())
        {
            more++;
            handler.releaseFrame(f->index);
        }
        else
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(more == clip.frames - (int)held.size());
    pipeline.stopFetching();
    std::filesystem::remove(clip.path);
}

TEST_CASE("native buffer: initialize() rejects a buffer that can not be native")
{
    testmedia::Clip clip{caseFileName("native_invalid"), AV_PIX_FMT_YUV420P, 64, 36, 2, 25};
    if (!writeClip(clip))
        return;
    AlternativeLocation alt;
    alt.uri = clip.path;
    MediaInfo media;
    media.alternatives.push_back(alt);
    auto stateWith = [&](BufferHandler &handler, uint32_t width, uint32_t height) {
        BufferInfo info{};
        info.handler = &handler;
        info.componentType = UNSIGNED_BYTE;
        info.type = SCALAR;
        info.outputMedia.width = width;
        info.outputMedia.height = height;
        info.outputMedia.format = G8_B8R8_2PLANE_420_UNORM;
        info.outputMedia.memory = FRAME_MEMORY_NATIVE;
        AvMediaPipeline pipeline;
        pipeline.initialize(media, {info});
        return pipeline.state();
    };
    BufferHandler cpu;
    cpu.allocate(4, 64 * 36 * 2); // not allocated with allocateNative()
    CHECK(stateWith(cpu, 64, 36) == ERROR);
    cpu.free();
    BufferHandler native;
    native.allocateNative(4);
    CHECK(stateWith(native, 0, 0) == ERROR); // not a texture
    CHECK(stateWith(native, 64, 36) == READY);
    std::filesystem::remove(clip.path);
}

#endif

#if (defined(__APPLE__) || defined(__ANDROID__)) && AVPIPELINE_HWDECODE

// Plays the clip with the software decoder, then with the hardware decoder required, and checks that the pictures
// stored are the same, within tolerance (for codecs whose decoding is not bit exact)
void compareHardwareWithSoftware(const testmedia::Clip &clip, ColorFormat format, int tolerance = 0)
{
    const Layout layout = layoutOf(format);
    const size_t bytes = storedBytes(layout, clip.width, clip.height);
    std::map<int, std::vector<uint8_t>> software;
    setenv("AVPIPELINE_HWDECODE", "0", 1);
    play(clip, format, clip.width, clip.height, [&](Frame *f, int k) {
        CHECK(f->length == bytes);
        software[k].assign(f->data, f->data + std::min<size_t>(f->length, bytes));
    });
    int compared = 0;
    setenv("AVPIPELINE_HWDECODE", "require", 1);
    play(clip, format, clip.width, clip.height, [&](Frame *f, int k) {
        CHECK(f->length == bytes);
        auto it = software.find(k);
        if (it == software.end() || f->length != bytes)
            return;
        int worst = 0;
        for (size_t i = 0; i < bytes; i++)
            worst = std::max(worst, std::abs((int)f->data[i] - (int)it->second[i]));
        if (worst > tolerance)
            std::fprintf(stderr, "  frame %d: hardware and software pictures differ by up to %d\n", k, worst);
        CHECK(worst <= tolerance);
        compared++;
    });
    unsetenv("AVPIPELINE_HWDECODE");
    CHECK(compared == clip.frames);
}

TEST_CASE("hardware decoding: required hardware decoding fails for a codec without hardware decoder")
{
    testmedia::Clip clip{caseFileName("ffv1_no_hardware"), AV_PIX_FMT_YUV420P, 64, 36, 2, 25};
    if (!writeClip(clip))
        return;
    BufferHandler handler;
    handler.allocate(4, (uint32_t)storedBytes(layoutOf(G8_B8R8_2PLANE_420_UNORM), 64, 36));
    BufferInfo info{};
    info.handler = &handler;
    info.componentType = UNSIGNED_BYTE;
    info.type = SCALAR;
    info.outputMedia.width = 64;
    info.outputMedia.height = 36;
    info.outputMedia.format = G8_B8R8_2PLANE_420_UNORM;
    AlternativeLocation alt;
    alt.uri = clip.path;
    MediaInfo media;
    media.alternatives.push_back(alt);

    setenv("AVPIPELINE_HWDECODE", "require", 1);
    AvMediaPipeline pipeline;
    pipeline.initialize(media, {info});
    unsetenv("AVPIPELINE_HWDECODE");
    CHECK(pipeline.state() == ERROR);
    std::filesystem::remove(clip.path);
}

#ifdef __APPLE__

TEST_CASE("hardware decoding: H.264 decoded with VideoToolbox is stored like the software decoder stores it")
{
    testmedia::Clip clip{caseFileName("h264_videotoolbox"), AV_PIX_FMT_YUV420P, 320, 180, 10, 25};
    std::string err = testmedia::write(clip, "h264_videotoolbox", 2000000);
    CHECK(err.empty());
    if (!err.empty())
    {
        std::fprintf(stderr, "  %s\n", err.c_str());
        return;
    }
    compareHardwareWithSoftware(clip, G8_B8R8_2PLANE_420_UNORM);
    compareHardwareWithSoftware(clip, G8_B8_R8_3PLANE_420_UNORM);
    std::filesystem::remove(clip.path);
}

TEST_CASE("hardware decoding: 10-bit HEVC decoded with VideoToolbox is stored like the software decoder stores it")
{
    testmedia::Clip clip{caseFileName("hevc_main10_videotoolbox"), AV_PIX_FMT_YUV420P10LE, 320, 180, 10, 25, AV_PIX_FMT_P010LE};
    std::string err = testmedia::write(clip, "hevc_videotoolbox", 2000000);
    CHECK(err.empty());
    if (!err.empty())
    {
        std::fprintf(stderr, "  %s\n", err.c_str());
        return;
    }
    compareHardwareWithSoftware(clip, G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16);
    std::filesystem::remove(clip.path);
}

#endif // __APPLE__

#if defined(__APPLE__) && defined(MAF_NATIVE_FRAMES)

// Plays the clip into a CPU buffer with the software decoder, then into a native buffer with VideoToolbox, the
// pictures handed over or converted without going through system memory, and checks that they store the same samples
void compareNativeHardwareWithCpu(const testmedia::Clip &clip, ColorFormat format, int tolerance)
{
    const Layout cpuLayout = layoutOf(format);
    const Layout nativeLayout = layoutOf(appleStoredFormat(format));
    const int bps = cpuLayout.depth > 8 ? 2 : 1;
    std::map<int, std::vector<uint8_t>> cpu;
    setenv("AVPIPELINE_HWDECODE", "0", 1);
    play(clip, format, clip.width, clip.height, [&](Frame *f, int k) {
        cpu[k].assign(f->data, f->data + f->length);
    });
    // the 3-plane layout of the CPU buffer, as the 2-plane layout of the native picture
    auto toNativeLayout = [&](const std::vector<uint8_t> &planes) {
        if (cpuLayout.twoPlanes == nativeLayout.twoPlanes)
            return planes;
        const int cw = (clip.width + cpuLayout.sx - 1) / cpuLayout.sx, ch = (clip.height + cpuLayout.sy - 1) / cpuLayout.sy;
        const size_t luma = (size_t)clip.width * clip.height * bps, chroma = (size_t)cw * ch * bps;
        std::vector<uint8_t> out(planes.begin(), planes.begin() + luma);
        for (size_t i = 0; i < chroma; i += bps)
            for (int c = 0; c < 2; c++)
                out.insert(out.end(), planes.begin() + luma + c * chroma + i, planes.begin() + luma + c * chroma + i + bps);
        return out;
    };
    int compared = 0;
    setenv("AVPIPELINE_HWDECODE", "require", 1);
    play(clip, format, clip.width, clip.height, [&](Frame *f, int k) {
        CVPixelBufferRef buffer = (CVPixelBufferRef)f->data;
        CHECK(buffer && CVPixelBufferGetIOSurface(buffer) != nullptr);
        auto it = cpu.find(k);
        if (!buffer || it == cpu.end())
            return;
        std::vector<uint8_t> native = readPlanes(buffer, nativeLayout);
        std::vector<uint8_t> expected = toNativeLayout(it->second);
        CHECK(native.size() == expected.size());
        if (native.size() != expected.size())
            return;
        int worst = 0;
        for (size_t i = 0; i < native.size(); i += bps)
        {
            int a = bps == 2 ? (native[i] | (native[i + 1] << 8)) >> 6 : native[i];
            int b = bps == 2 ? (expected[i] | (expected[i + 1] << 8)) >> 6 : expected[i];
            worst = std::max(worst, std::abs(a - b));
        }
        if (worst > tolerance)
            std::fprintf(stderr, "  frame %d: native and CPU pictures differ by up to %d\n", k, worst);
        CHECK(worst <= tolerance);
        compared++;
    }, Memory::NATIVE);
    unsetenv("AVPIPELINE_HWDECODE");
    CHECK(compared == clip.frames);
}

TEST_CASE("native buffer from VideoToolbox: H.264 pictures are handed over or converted without system memory")
{
    testmedia::Clip clip{caseFileName("native_h264_videotoolbox"), AV_PIX_FMT_YUV420P, 320, 180, 10, 25};
    std::string err = testmedia::write(clip, "h264_videotoolbox", 2000000);
    CHECK(err.empty());
    if (!err.empty())
        return;
    compareNativeHardwareWithCpu(clip, G8_B8R8_2PLANE_420_UNORM, 0);  // the decoded picture itself
    compareNativeHardwareWithCpu(clip, G8_B8_R8_3PLANE_420_UNORM, 0); // planes rearranged
    std::filesystem::remove(clip.path);
}

TEST_CASE("native buffer from VideoToolbox: 10-bit HEVC pictures are handed over")
{
    testmedia::Clip clip{caseFileName("native_hevc_main10_videotoolbox"), AV_PIX_FMT_YUV420P10LE, 320, 180, 10, 25, AV_PIX_FMT_P010LE};
    std::string err = testmedia::write(clip, "hevc_videotoolbox", 2000000);
    CHECK(err.empty());
    if (!err.empty())
        return;
    compareNativeHardwareWithCpu(clip, G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, 0);
    std::filesystem::remove(clip.path);
}

TEST_CASE("native buffer from VideoToolbox: RGBA pictures are converted without system memory")
{
    testmedia::Clip clip{caseFileName("native_rgba_videotoolbox"), AV_PIX_FMT_YUV420P, 320, 180, 5, 25};
    std::string err = testmedia::write(clip, "h264_videotoolbox", 2000000);
    CHECK(err.empty());
    if (!err.empty())
        return;
    setenv("AVPIPELINE_HWDECODE", "require", 1);
    int frames = play(clip, RGBA, 320, 180, [&](Frame *f, int) {
        CVPixelBufferRef buffer = (CVPixelBufferRef)f->data;
        CHECK(buffer && CVPixelBufferGetPixelFormatType(buffer) == kCVPixelFormatType_32BGRA && CVPixelBufferGetIOSurface(buffer) != nullptr);
    }, Memory::NATIVE);
    unsetenv("AVPIPELINE_HWDECODE");
    CHECK(frames == clip.frames);
    std::filesystem::remove(clip.path);
}

TEST_CASE("native buffer from VideoToolbox: require fails for a picture that goes through system memory")
{
    testmedia::Clip clip{caseFileName("native_red_videotoolbox"), AV_PIX_FMT_YUV420P, 320, 180, 5, 25};
    std::string err = testmedia::write(clip, "h264_videotoolbox", 2000000);
    CHECK(err.empty());
    if (!err.empty())
        return;
    BufferHandler handler;
    handler.allocateNative(4);
    BufferInfo info{};
    info.handler = &handler;
    info.componentType = UNSIGNED_BYTE;
    info.type = SCALAR;
    info.outputMedia.width = 320;
    info.outputMedia.height = 180;
    info.outputMedia.format = RED; // one component, taken from the RGB picture in system memory
    info.outputMedia.memory = FRAME_MEMORY_NATIVE;
    AlternativeLocation alt;
    alt.uri = clip.path;
    MediaInfo media;
    media.alternatives.push_back(alt);
    setenv("AVPIPELINE_HWDECODE", "require", 1);
    AvMediaPipeline pipeline;
    pipeline.initialize(media, {info});
    CHECK(pipeline.state() == READY);
    pipeline.startFetching(TimeInfo{0.0, 0.0, true, false}, ViewInfo{});
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (pipeline.state() != ERROR && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    unsetenv("AVPIPELINE_HWDECODE");
    CHECK(pipeline.state() == ERROR);
    CHECK(handler.readFrame() == nullptr);
    std::filesystem::remove(clip.path);
}

#endif // __APPLE__ && MAF_NATIVE_FRAMES

#ifdef __ANDROID__

TEST_CASE("hardware decoding: MPEG-4 decoded with MediaCodec is stored like the software decoder stores it")
{
    // intra pictures only: MPEG-4 Part 2 decoders are not bit exact, a difference must not drift across pictures
    testmedia::Clip clip{caseFileName("mpeg4_mediacodec"), AV_PIX_FMT_YUV420P, 320, 180, 10, 25};
    std::string err = testmedia::write(clip, "mpeg4", 4000000, 1);
    CHECK(err.empty());
    if (!err.empty())
    {
        std::fprintf(stderr, "  %s\n", err.c_str());
        return;
    }
    compareHardwareWithSoftware(clip, G8_B8R8_2PLANE_420_UNORM, 4);
    std::filesystem::remove(clip.path);
}

#endif // __ANDROID__

#endif

} // namespace



int main()
{
    av_log_set_level(AV_LOG_ERROR);
    return check::run();
}
