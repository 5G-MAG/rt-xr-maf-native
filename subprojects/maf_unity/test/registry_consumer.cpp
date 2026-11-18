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

// The frame lifetime of the maf_unity plugin, seen from Unity: textures show frames of a native buffer, and the frames
// no texture shows are released to the buffer once the GPU completed the commands that used them.

#include <TextureRegistry.hpp>

#include "check.hpp"

using namespace MAF;

namespace {

int fakePictures[8];

// Records the imports and the releases of the pictures
struct FakeImporter : PictureImporter
{
    int imports = 0;
    int released = 0;
    bool fail = false;
    std::shared_ptr<ImportedPicture> Import(void *picture, const ImportParams &params) override
    {
        if (fail)
            return nullptr;
        imports++;
        auto p = std::make_shared<ImportedPicture>();
        p->params = params;
        p->planeCount = 2;
        p->texture[0] = picture;
        p->release = [this] { released++; };
        return p;
    }
};

// A native buffer whose frames 0..n-1 were written by a producer and read (held) by the consumer
void fill(BufferHandler &h, int n)
{
    for (int i = 0; i < n; i++)
    {
        Frame *f = &h.getFrames()->at(h.getWriteIdx());
        f->data = (uint8_t *)&fakePictures[i];
        f->length = 0;
        f->timestamp = getTimestamp((double)i);
        h.writeFrame2(f);
    }
    for (int i = 0; i < n; i++)
        h.readFrame();
}

TextureRegistry registryFor()
{
    return TextureRegistry([](BufferHandler *h, uint8_t i) { h->releaseFrame(i); });
}

} // namespace

TEST_CASE("a frame shown on a texture is released once another frame is shown and the GPU completed")
{
    BufferHandler h;
    h.allocateNative(4);
    fill(h, 2);
    FakeImporter importer;
    TextureRegistry r = registryFor();
    int t = r.Create();

    const ImportedPicture *p = r.Submit(t, &h, 0, &fakePictures[0], 64, 36, importer);
    CHECK(p != nullptr && p->planeCount == 2 && p->texture[0] == &fakePictures[0]);
    CHECK(r.Current(t) == p);
    CHECK(r.Submit(t, &h, 0, &fakePictures[0], 64, 36, importer) == p); // the same frame again: nothing changes
    CHECK(importer.imports == 1);

    CHECK(r.Submit(t, &h, 1, &fakePictures[1], 64, 36, importer) != nullptr);
    CHECK(r.PendingFrames() == 1);
    CHECK(!h.isWritable(0)); // frame 0 is still held: the GPU may use it
    uint64_t batch = r.RetireBatch();
    CHECK(batch != 0);
    CHECK(r.RetireBatch() == 0);
    CHECK(!h.isWritable(0));
    CHECK(importer.released == 0);

    r.Complete(batch); // the GPU completed the commands recorded before the batch
    CHECK(importer.released == 1);
    CHECK(r.PendingFrames() == 0);
    h.free();
}

TEST_CASE("a frame shown on two textures is imported once and released when neither shows it")
{
    BufferHandler h;
    h.allocateNative(4);
    fill(h, 3);
    FakeImporter importer;
    TextureRegistry r = registryFor();
    int a = r.Create(), b = r.Create();
    CHECK(r.Submit(a, &h, 0, &fakePictures[0], 64, 36, importer) == r.Submit(b, &h, 0, &fakePictures[0], 64, 36, importer));
    CHECK(importer.imports == 1);
    CHECK(r.ShownFrames() == 1);

    r.Submit(a, &h, 1, &fakePictures[1], 64, 36, importer);
    CHECK(r.RetireBatch() == 0); // b still shows frame 0
    r.Submit(b, &h, 1, &fakePictures[1], 64, 36, importer);
    uint64_t batch = r.RetireBatch();
    CHECK(batch != 0);
    r.Complete(batch);
    CHECK(importer.released == 1);
    CHECK(importer.imports == 2);
    h.free();
}

TEST_CASE("released frames can be written again by the producer")
{
    BufferHandler h;
    h.allocateNative(3);
    fill(h, 3); // the consumer holds every frame: the producer is stopped
    CHECK(!h.isWritable(h.getWriteIdx()));
    FakeImporter importer;
    TextureRegistry r = registryFor();
    int t = r.Create();
    r.Submit(t, &h, 0, &fakePictures[0], 64, 36, importer);
    r.Submit(t, &h, 1, &fakePictures[1], 64, 36, importer);
    h.releaseFrame(1); // the consumer releases a skipped frame itself
    r.Submit(t, &h, 2, &fakePictures[2], 64, 36, importer);
    r.Complete(r.RetireBatch()); // frames 0 and 1 no longer shown
    CHECK(h.isWritable(0));
    h.free();
}

TEST_CASE("destroying a texture retires its frame; a failed import keeps the frame shown")
{
    BufferHandler h;
    h.allocateNative(4);
    fill(h, 2);
    FakeImporter importer;
    TextureRegistry r = registryFor();
    int t = r.Create();
    const ImportedPicture *p = r.Submit(t, &h, 0, &fakePictures[0], 64, 36, importer);
    importer.fail = true;
    CHECK(r.Submit(t, &h, 1, &fakePictures[1], 64, 36, importer) == nullptr);
    CHECK(r.Current(t) == p);
    r.Destroy(t);
    CHECK(r.Current(t) == nullptr);
    r.Complete(r.RetireBatch());
    CHECK(importer.released == 1);
    h.free();
}

TEST_CASE("the frames of a forgotten buffer are not released to it")
{
    int releases = 0;
    TextureRegistry r([&](BufferHandler *, uint8_t) { releases++; });
    BufferHandler h;
    FakeImporter importer;
    int t = r.Create();
    r.Submit(t, &h, 0, &fakePictures[0], 64, 36, importer);
    r.Submit(t, &h, 1, &fakePictures[1], 64, 36, importer);
    uint64_t batch = r.RetireBatch();
    r.Submit(t, &h, 2, &fakePictures[2], 64, 36, importer);
    r.ForgetBuffer(&h); // the buffer is about to be freed
    r.Complete(batch);
    r.Destroy(t);
    r.Complete(r.RetireBatch());
    CHECK(releases == 0);
    CHECK(importer.released == 3); // the pictures are released all the same
}

TEST_CASE("the frame size and the color conversion of the texture are given to the importer")
{
    BufferHandler h;
    FakeImporter importer;
    TextureRegistry r([](BufferHandler *, uint8_t) {});
    int t = r.Create();
    TextureRegistry::Settings settings;
    settings.ycbcrModel = 2; // VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709
    settings.ycbcrRange = 1; // VK_SAMPLER_YCBCR_RANGE_ITU_NARROW
    settings.flipY = true;
    CHECK(r.SetSettings(t, settings));
    CHECK(!r.SetSettings(t + 100, settings));
    const ImportedPicture *p = r.Submit(t, &h, 0, &fakePictures[0], 1920, 1080, importer);
    CHECK(p && p->params.width == 1920 && p->params.height == 1080 && p->params.ycbcrModel == 2 && p->params.ycbcrRange == 1);
    CHECK(r.GetSettings(t).flipY);
    r.Destroy(t);
    r.Complete(r.RetireBatch());
}

int main()
{
    return check::run();
}
