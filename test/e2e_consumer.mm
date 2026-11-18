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

// End to end, seen from Unity without its renderer: VideoToolbox decodes into a native frame buffer, the maf_unity
// plugin imports each frame as Metal textures and releases it once a (simulated) GPU completion says it is no longer
// used, and the pipeline goes on with the released frames.

#import <Metal/Metal.h>
#include <CoreVideo/CoreVideo.h>

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

#include <AvPipeline.hpp>
#include <Importers.hpp>
#include <MafUnity.h>
#include <TextureRegistry.hpp>

#include "check.hpp"
#include "test_media.hpp"

using namespace MAF;

TEST_CASE("VideoToolbox frames are shown as Metal textures and released back to the pipeline")
{
    const std::string path = (std::filesystem::temp_directory_path() / "maf_e2e_h264.mkv").string();
    testmedia::Clip clip{path, AV_PIX_FMT_YUV420P, 320, 180, 30, 25};
    std::string err = testmedia::write(clip, "h264_videotoolbox", 2000000);
    CHECK(err.empty());
    if (!err.empty())
        return;
    std::unique_ptr<PictureImporter> importer = CreateMetalImporter(MTLCreateSystemDefaultDevice());
    CHECK(importer != nullptr);
    if (!importer)
        return;

    BufferHandler handler;
    handler.allocateNative(4);
    BufferInfo info{};
    info.handler = &handler;
    info.componentType = UNSIGNED_BYTE;
    info.type = SCALAR;
    info.outputMedia.width = 320;
    info.outputMedia.height = 180;
    info.outputMedia.format = G8_B8R8_2PLANE_420_UNORM;
    info.outputMedia.memory = FRAME_MEMORY_NATIVE;
    AlternativeLocation alt;
    alt.uri = clip.path;
    MediaInfo media;
    media.alternatives.push_back(alt);

    // the pictures are VideoToolbox's own: they do not go through system memory
    setenv("AVPIPELINE_HWDECODE", "require", 1);
    AvMediaPipeline pipeline;
    pipeline.initialize(media, {info});
    CHECK(pipeline.state() == READY);
    pipeline.startFetching(TimeInfo{0.0, 0.0, true, false}, ViewInfo{});

    TextureRegistry registry([](BufferHandler *h, uint8_t i) { h->releaseFrame(i); });
    const int texture = registry.Create();
    std::vector<uint64_t> inFlight; // batches the "GPU" completes two frames later
    int shown = 0;
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (shown < clip.frames && std::chrono::steady_clock::now() < until)
    {
        Frame *f = handler.readFrame();
        if (!f)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        const ImportedPicture *p = registry.Submit(texture, &handler, f->index, f->data, *importer);
        CHECK(p != nullptr);
        if (!p)
            break;
        CHECK(p->planeCount == 2 && p->format[0] == MAFUNITY_FORMAT_R8 && p->format[1] == MAFUNITY_FORMAT_RG8);
        id<MTLTexture> luma = (__bridge id<MTLTexture>)p->texture[0];
        CHECK(luma.width == 320 && luma.height == 180);
        shown++;
        // the render event after the frame: the frames retired so far are released once the GPU completes
        if (uint64_t batch = registry.RetireBatch())
            inFlight.push_back(batch);
        if (inFlight.size() > 2)
        {
            registry.Complete(inFlight.front());
            inFlight.erase(inFlight.begin());
        }
    }
    unsetenv("AVPIPELINE_HWDECODE");
    // more frames than the buffer holds: the released frames were written again
    CHECK(shown == clip.frames);
    CHECK(pipeline.state() != ERROR);
    pipeline.stopFetching();
    registry.Destroy(texture);
    for (uint64_t batch : inFlight)
        registry.Complete(batch);
    registry.Complete(registry.RetireBatch());
    std::filesystem::remove(clip.path);
}

int main()
{
    @autoreleasepool
    {
        return check::run();
    }
}
