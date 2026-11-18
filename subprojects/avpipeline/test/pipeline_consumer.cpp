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
#include <filesystem>
#include <functional>
#include <algorithm>
#include <set>
#include <thread>

#include <AvPipeline.hpp>

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

// Plays the clip into a CPU texture buffer of the given format and maximum size, and calls onFrame with each frame
// read and the index of its picture in the clip. Returns the number of pictures seen.
int play(const testmedia::Clip &clip, ColorFormat format, int maxWidth, int maxHeight, const std::function<void(Frame *, int)> &onFrame)
{
    const Layout layout = layoutOf(format);
    BufferHandler handler;
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

} // namespace


int main()
{
    av_log_set_level(AV_LOG_ERROR);
    return check::run();
}
