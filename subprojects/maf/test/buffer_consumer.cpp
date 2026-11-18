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

// Buffer API (ISO/IEC 23090-14, clause 6.3) seen from the consumer (the Presentation Engine): the test allocates a
// buffer, writes frames the way the avpipeline decoders do (in place, at the write index) and reads them back.

#include <cstring>

#include <maf.hpp>

#include "check.hpp"

using namespace MAF;

namespace {

const uint32_t kFrameSize = 16;

// Writes a frame in place at the write index, as the decoders of avpipeline do. Returns false when the buffer
// does not let the frame be written.
bool produce(BufferHandler &h, double time_s, uint8_t fill)
{
    int idx = h.getWriteIdx();
    if (!h.isWritable(idx)) {
        return false;
    }
    Frame *f = &h.getFrames()->at(idx);
    std::memset(f->data, fill, kFrameSize);
    f->timestamp = getTimestamp(time_s);
    f->length = kFrameSize;
    h.writeFrame2(f);
    return true;
}

} // namespace

TEST_CASE("cpu buffer: frames are read in the order they are written")
{
    BufferHandler h;
    h.allocate(4, kFrameSize);
    CHECK(h.readFrame() == nullptr);

    CHECK(produce(h, 1.0, 0x11));
    CHECK(produce(h, 2.0, 0x22));
    CHECK(produce(h, 3.0, 0x33));
    CHECK(h.count() == 3);

    const uint8_t expected[] = {0x11, 0x22, 0x33};
    for (int i = 0; i < 3; i++) {
        Frame *f = h.readFrame();
        CHECK(f != nullptr);
        if (f) {
            CHECK(f->index == i);
            CHECK(f->length == kFrameSize);
            CHECK(f->data[0] == expected[i] && f->data[kFrameSize - 1] == expected[i]);
            CHECK(getTime_s(f->timestamp) > i + 0.99 && getTime_s(f->timestamp) < i + 1.01);
        }
    }
    CHECK(h.readFrame() == nullptr);
    h.free();
}

TEST_CASE("cpu buffer: a full buffer refuses writes until a frame is read")
{
    BufferHandler h;
    h.allocate(4, kFrameSize);
    for (int i = 0; i < 4; i++) {
        CHECK(produce(h, i, (uint8_t)i));
    }
    CHECK(!h.isWritable(h.getWriteIdx()));
    CHECK(!produce(h, 4.0, 4));
    CHECK_THROWS(h.writeFrame2(&h.getFrames()->at(h.getWriteIdx())));

    // the first read makes frame 0 the presentation frame: it is still not writable
    CHECK(h.readFrame() != nullptr);
    CHECK(!h.isWritable(0));
    // once the presentation moves on, frame 0 can be written again
    CHECK(h.readFrame() != nullptr);
    CHECK(h.isWritable(0));
    CHECK(produce(h, 4.0, 4));
    h.free();
}

TEST_CASE("cpu buffer: readFrame(time) returns the latest frame not after the time, without consuming it")
{
    BufferHandler h;
    h.allocate(4, kFrameSize);
    CHECK(produce(h, 1.0, 1));
    CHECK(produce(h, 2.0, 2));
    CHECK(produce(h, 3.0, 3));

    Frame *f = h.readFrame(2.5);
    CHECK(f != nullptr && f->data[0] == 2);
    CHECK(h.count() == 3);
    CHECK(h.readFrame(0.5) == nullptr);
    f = h.readFrame(10.0);
    CHECK(f != nullptr && f->data[0] == 3);

    uint64_t ts = getTimestamp(1.0);
    f = h.readFrame(&ts);
    CHECK(f != nullptr && f->data[0] == 1);
    h.free();
}

TEST_CASE("cpu buffer: frames are not held, releaseFrame does not change what can be written")
{
    BufferHandler h;
    h.allocate(3, kFrameSize);
    CHECK(produce(h, 1.0, 1));
    CHECK(produce(h, 2.0, 2));
    CHECK(h.readFrame() != nullptr);
    CHECK(h.readFrame() != nullptr); // frame 0 is no longer presented
    CHECK(h.getWriteIdx() == 2);
    CHECK(produce(h, 3.0, 3));
    CHECK(h.getWriteIdx() == 0);
    CHECK(h.isWritable(0));

    h.releaseFrame(0);
    h.releaseFrame(1);
    h.releaseFrame(200); // out of range: ignored
    CHECK(h.isWritable(0));
    CHECK(produce(h, 4.0, 4));
    h.free();
}

TEST_CASE("buffer: nothing is writable before allocation and after free")
{
    BufferHandler h;
    CHECK(!h.isWritable(0));
    h.releaseFrame(0); // not allocated: ignored
    h.allocate(2, kFrameSize);
    CHECK(h.isWritable(0));
    CHECK(!h.isWritable(2));
    CHECK(!h.isWritable(-1));
    h.free();
    CHECK(!h.isWritable(0));
    CHECK_THROWS(h.writeFrame2(nullptr));
}

#ifdef MAF_NATIVE_FRAMES

namespace {

// Stand-ins for the picture handles of a native buffer: the buffer only stores and returns the pointer. free() must
// not release them: these handles are not heap allocated, so a call to std::free on them would crash the test.
int fakePictures[8];

void *handle(int i)
{
    return &fakePictures[i];
}

// Writes a native frame in place at the write index, as a media pipeline does: Frame.data is the handle, length is 0.
bool produceNative(BufferHandler &h, double time_s, int picture)
{
    int idx = h.getWriteIdx();
    if (!h.isWritable(idx)) {
        return false;
    }
    Frame *f = &h.getFrames()->at(idx);
    f->data = (uint8_t *)handle(picture);
    f->length = 0;
    f->timestamp = getTimestamp(time_s);
    f->extraFrameInfoFlags = 0x01;
    f->width = 64;
    f->height = 32;
    h.writeFrame2(f);
    return true;
}

} // namespace

TEST_CASE("native buffer: frames carry the handle written by the producer")
{
    BufferHandler h;
    h.allocateNative(4);
    CHECK(h.getFrames()->at(0).data == nullptr);
    CHECK(produceNative(h, 1.0, 1));
    CHECK(produceNative(h, 2.0, 2));

    Frame *f = h.readFrame();
    CHECK(f != nullptr && f->data == handle(1) && f->length == 0 && f->width == 64 && f->height == 32);
    f = h.readFrame();
    CHECK(f != nullptr && f->data == handle(2));
    h.free();
}

TEST_CASE("native buffer: a frame read is not overwritten until it is released")
{
    BufferHandler h;
    h.allocateNative(3);
    CHECK(produceNative(h, 1.0, 1));
    CHECK(produceNative(h, 2.0, 2));
    CHECK(produceNative(h, 3.0, 3));

    // the consumer presents frame 0, then frame 1: on a CPU buffer frame 0 could be written again
    CHECK(h.readFrame() != nullptr);
    CHECK(h.readFrame() != nullptr);
    CHECK(h.getWriteIdx() == 0);
    CHECK(!h.isWritable(0));
    CHECK(!produceNative(h, 4.0, 4));
    CHECK_THROWS(h.writeFrame2(&h.getFrames()->at(0)));

    // the consumer is done with frame 0 (e.g. the GPU finished reading it)
    h.releaseFrame(0);
    CHECK(h.isWritable(0));
    CHECK(produceNative(h, 4.0, 4));
    // frame 1 is held, and it is the presentation frame: it is protected either way
    CHECK(h.getWriteIdx() == 1);
    CHECK(!h.isWritable(1));
    h.releaseFrame(1);
    CHECK(!h.isWritable(1));
    // once frame 2 is presented and frame 1 is released, frame 1 can be written
    CHECK(h.readFrame() != nullptr);
    CHECK(h.isWritable(1));
    CHECK(produceNative(h, 5.0, 5));
    h.free();
}

TEST_CASE("native buffer: readFrame(time) holds the frame it returns, reading it again holds it once")
{
    BufferHandler h;
    h.allocateNative(3);
    CHECK(produceNative(h, 1.0, 1));
    CHECK(produceNative(h, 2.0, 2));

    Frame *f = h.readFrame(1.5);
    CHECK(f != nullptr && f->data == handle(1));
    f = h.readFrame(1.6); // the same frame, presented again
    CHECK(f != nullptr && f->index == 0);
    // readFrame(time) does not consume frames: readFrame() moves the presentation to the next frame
    f = h.readFrame();
    CHECK(f != nullptr && f->index == 1);
    CHECK(produceNative(h, 3.0, 3));
    CHECK(h.getWriteIdx() == 0);
    CHECK(!h.isWritable(0)); // no longer presented, but still held
    h.releaseFrame(0); // one release is enough
    CHECK(h.isWritable(0));
    h.free();
}

TEST_CASE("native buffer: free() leaves the handles to their owner, allocate() makes a CPU buffer again")
{
    BufferHandler h;
    h.allocateNative(2);
    CHECK(produceNative(h, 1.0, 1));
    CHECK(h.readFrame() != nullptr);
    h.free(); // would crash if it freed the fake handles

    h.allocate(2, kFrameSize);
    CHECK(produce(h, 1.0, 1));
    CHECK(h.readFrame() != nullptr);
    CHECK(produce(h, 2.0, 2));
    CHECK(h.readFrame() != nullptr);
    CHECK(h.isWritable(0)); // not held
    h.free();
}

#else

TEST_CASE("native buffer: not supported by this build")
{
    BufferHandler h;
    CHECK_THROWS(h.allocateNative(2));
}

#endif

int main()
{
    return check::run();
}
