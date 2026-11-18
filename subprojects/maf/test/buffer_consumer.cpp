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

int main()
{
    return check::run();
}
