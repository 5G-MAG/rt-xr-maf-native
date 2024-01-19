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


#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <thread>
#include <maf.hpp>

using namespace MAF;

#define BACKBUFFER(bh) &(bh.getFrames()->at(bh.getWriteIdx()));


TEST_CASE("given count=2; write, read, write, read"){

    BufferHandler bh;
    uint8_t cap = 2;
    bh.allocate(cap);
    
    Frame backBuffer;
    Frame* frontBuffer;

    // write
    backBuffer.timestamp = 1;
    bh.writeFrame(&backBuffer);

    // read
    frontBuffer = bh.readFrame();
    REQUIRE(frontBuffer != nullptr);
    REQUIRE(frontBuffer->timestamp == 1);
    REQUIRE(bh.readFrame() == nullptr);
    CHECK(bh.count() == 0);

    // write
    backBuffer.timestamp = 2;
    bh.writeFrame(&backBuffer);
    CHECK(bh.count() == 1);

    // read
    frontBuffer = bh.readFrame();
    REQUIRE(frontBuffer != nullptr);
    REQUIRE(frontBuffer->timestamp == 2);

    CHECK(bh.count() == 0);
    REQUIRE(bh.readFrame() == nullptr);
    CHECK(bh.count() == 0);

    bh.free();
};

TEST_CASE("given count=2; write, write, read"){

    BufferHandler bh;
    uint8_t cap = 2;
    bh.allocate(cap);
    
    Frame backBuffer;
    Frame* frontBuffer;
    
    // write
    backBuffer.timestamp = 1;
    bh.writeFrame(&backBuffer);
    CHECK(bh.count() == 1);

    // write
    backBuffer.timestamp = 2;
    bh.writeFrame(&backBuffer);
    CHECK(bh.count() == 1);

    // read
    frontBuffer = nullptr;
    frontBuffer = bh.readFrame();
    REQUIRE(frontBuffer != nullptr);
    REQUIRE(frontBuffer->timestamp == 1);
    CHECK(bh.count() == 0);

    bh.free();
}


TEST_CASE("given count=2; write, read, write, write, write, read"){

    BufferHandler bh;
    uint8_t cap = 2;
    bh.allocate(cap);
    
    Frame backBuffer;
    Frame* frontBuffer;
    
    // write
    backBuffer.timestamp = 1;
    bh.writeFrame(&backBuffer);
    CHECK(bh.count() == 1);

    // read
    frontBuffer = nullptr;
    frontBuffer = bh.readFrame();
    REQUIRE(frontBuffer != nullptr);
    REQUIRE(frontBuffer->timestamp == 1);

    CHECK(bh.count() == 0);
    REQUIRE(bh.readFrame() == nullptr);

    // write
    backBuffer.timestamp = 2;
    bh.writeFrame(&backBuffer);
    CHECK(bh.count() == 1);
    // write
    backBuffer.timestamp = 3;
    bh.writeFrame(&backBuffer);
    CHECK(bh.count() == 1);
    // write
    backBuffer.timestamp = 4;
    bh.writeFrame(&backBuffer);
    CHECK(bh.count() == 1);

    // read
    frontBuffer = nullptr;
    frontBuffer = bh.readFrame();
    REQUIRE(frontBuffer != nullptr);
    REQUIRE(frontBuffer->timestamp == 4);
    
    CHECK(bh.count() == 0);
    REQUIRE(bh.readFrame() == nullptr);

    bh.free();
}


TEST_CASE("intial state, readFrame returns null"){

    BufferHandler bh;
    uint8_t cap = 2;
    bh.allocate(cap);

    // after writing frames
    CHECK(bh.getReadIdx() == 0);
    CHECK(bh.getWriteIdx() == 0);
    REQUIRE(bh.readFrame() == nullptr);

    bh.free();
}

TEST_CASE("intial state, write 1 frame, read it, empty"){

    BufferHandler bh;
    uint8_t cap = 2;
    bh.allocate(cap);
    
    Frame* frontBuffer;
    Frame backBuffer;

    uint32_t length = 1024;
    uint8_t* data = new uint8_t[length];
    uint64_t timestamp =  1234567890;    
    backBuffer.timestamp = timestamp;
    backBuffer.length = length;
    backBuffer.data = data;
    
    bh.writeFrame(&backBuffer);
    REQUIRE(bh.getWriteIdx() == 1);
    frontBuffer = bh.readFrame();
    REQUIRE(frontBuffer != nullptr);
    REQUIRE(bh.getReadIdx() == 0);
    REQUIRE(frontBuffer->timestamp == timestamp);
    REQUIRE(frontBuffer->data == data);
    REQUIRE(frontBuffer->length == length);

    REQUIRE(bh.readFrame() == nullptr);

    // delete data;
    bh.free();
}


TEST_CASE("BufferHandler::allocate(5)"){
    BufferHandler bh;
    uint8_t cap = 5;
    bh.allocate(cap);
    REQUIRE(bh.getReadIdx() == 0);
    REQUIRE(bh.getWriteIdx() == 0);
    REQUIRE(bh.readFrame()== nullptr);

    std::vector<Frame>* frames = bh.getFrames();
    uint8_t idx = 0;
    for (auto &f : *frames) {
        REQUIRE(f.index == idx);
        REQUIRE(f.length == 0);
        REQUIRE(f.data == nullptr);
        REQUIRE(f.timestamp == 0);
        idx++;
    }
    REQUIRE(idx==cap);

    bh.free();
}


TEST_CASE("BufferHandler::writeFrame(F), given F the frame at writeIdx in buffer handler"){

    Frame* frontBuffer = nullptr;
    Frame* backBuffer = nullptr;
    BufferHandler bh;
    uint8_t cap = 5;

    bh.allocate(cap);

    REQUIRE(bh.getReadIdx() == 0);
    REQUIRE(bh.getWriteIdx() == 0);

    backBuffer = BACKBUFFER(bh);
    backBuffer->timestamp = ((backBuffer->index+1)*cap);
    bh.writeFrame(backBuffer);

    REQUIRE(bh.getReadIdx() == 0);
    REQUIRE(bh.getWriteIdx() == 1);

    frontBuffer = bh.readFrame();

    REQUIRE(frontBuffer == backBuffer);

    bh.free();
}


TEST_CASE("BufferHandler::writeFrame(F), given F was not allocated by the buffer handler"){

    BufferHandler bh;
    Frame backBuffer;
    Frame* frontBuffer = nullptr;
    
    size_t length = 2048;
    uint8_t* data = new uint8_t[length];
    uint64_t ts = 1234567890;
    uint8_t cap = 5;
    bh.allocate(cap);

    REQUIRE(bh.capacity() == cap);
    REQUIRE(bh.count() == 0);
    REQUIRE(bh.getReadIdx() == 0);
    REQUIRE(bh.getWriteIdx() == 0);

    backBuffer.index = 255;
    backBuffer.timestamp = ++ts;
    backBuffer.length = (uint32_t)length;
    backBuffer.data = data;

    bh.writeFrame(&backBuffer);

    REQUIRE(bh.getReadIdx() == 0);
    REQUIRE(bh.getWriteIdx() == 1);

    frontBuffer = bh.readFrame();
    REQUIRE(frontBuffer != &backBuffer);
    REQUIRE(frontBuffer->index == 0);
    REQUIRE(frontBuffer->timestamp == ts);
    REQUIRE(frontBuffer->length == length);
    REQUIRE(frontBuffer->data == data);
    
    delete[] data;
    frontBuffer->data = nullptr;
    bh.free();

}

TEST_CASE("BufferHandler::getWriteIdx() loops back to 0"){

    Frame backBuffer;
    Frame* frontBuffer = nullptr;
    BufferHandler bh;
    uint8_t cap = 5;
    bh.allocate(cap);

    for (uint8_t i = 0; i < cap; i++){
        REQUIRE(bh.getWriteIdx() == i);
        bh.writeFrame(&backBuffer);
        frontBuffer = bh.readFrame();
        REQUIRE(frontBuffer != nullptr);
    }
    REQUIRE(bh.getWriteIdx() == 0);

    bh.free();    
}


TEST_CASE("BufferHandler::writeFrame(F), writeIdx jumps over readIdx"){

    Frame* backBuffer = nullptr;

    BufferHandler bh;
    uint8_t cap = 5;
    bh.allocate(cap);

    for (uint8_t i = 0; i < cap-1; i++){
        CHECK(bh.count() == i);
        backBuffer = BACKBUFFER(bh);
        bh.writeFrame(backBuffer);
    }

    CHECK(bh.count() == (cap-1));
    REQUIRE(((bh.getWriteIdx()+1)%bh.capacity()) == bh.getReadIdx());
    backBuffer = BACKBUFFER(bh);
    bh.writeFrame(backBuffer);
    REQUIRE(((bh.getReadIdx()+1)%bh.capacity()) == bh.getWriteIdx());
    CHECK(bh.count() == 1);

    CHECK(bh.readFrame() != nullptr);
    for (uint8_t i = 0; i < cap-1; i++){
        backBuffer = BACKBUFFER(bh);
        bh.writeFrame(backBuffer);
    }
    REQUIRE(((bh.getReadIdx()+1)%bh.capacity()) == bh.getWriteIdx());
    CHECK(bh.count() == 0);

    bh.free();

}


TEST_CASE("BufferHandler::readFrame(), increments readIdx, returns Frame at readIdx"){

    Frame backBuffer;
    Frame* frontBuffer;
    BufferHandler bh;

    uint8_t cap = 5;
    bh.allocate(cap);
    
    for (uint8_t i = 0; i < 4; i++){
        bh.writeFrame(&backBuffer);
    }

    for (int i = 0; i < 4; i++){
        frontBuffer = bh.readFrame();
        REQUIRE(bh.getReadIdx() == i);
        REQUIRE(frontBuffer->index == i);
    }
    
}

TEST_CASE("BufferHandler::readFrame(), loops back to 0"){

    Frame backBuffer;
    Frame* frontBuffer;
    BufferHandler bh;

    uint8_t cap = 5;
    bh.allocate(cap);
    
    for (uint8_t i = 0; i < 4; i++){
        bh.writeFrame(&backBuffer);
    }

    for (uint8_t i = 0; i < 4; i++){
        frontBuffer = bh.readFrame();
        REQUIRE(frontBuffer->index == i);
    }

    bh.writeFrame(&backBuffer);
    bh.writeFrame(&backBuffer);
    frontBuffer = bh.readFrame();

}


TEST_CASE("BufferHandler::writeFrame2() throws when buffer is full"){
    Frame backBuffer;
    BufferHandler bh;

    uint8_t cap = 5;
    bh.allocate(cap);
    
    for (uint8_t i = 0; i < cap; i++){
        REQUIRE(bh.count() == i);
        bh.writeFrame2(&backBuffer);
    }

    REQUIRE(bh.count() == bh.capacity());
    REQUIRE_THROWS(bh.writeFrame2(&backBuffer));

    CHECK(bh.readFrame() != nullptr);
    REQUIRE_THROWS(bh.writeFrame2(&backBuffer));

    bh.free();
}

TEST_CASE("BufferHandler::writeFrame3() blocks as long as buffer is full"){
    
    BufferHandler bh;
    Frame frame;
    
    std::mutex m;
    std::condition_variable cv;

    bh.allocate(5);
    for (uint8_t i = 0; i < 4; i++){
        bh.writeFrame3(&frame);
        REQUIRE(bh.readFrame() != nullptr);
    }
    CHECK( bh.count() == 0 );
    for (uint8_t i = 0; i < 4; i++){
        bh.writeFrame3(&frame);
        CHECK(bh.count() == i+1);
    }
    CHECK(bh.count() == 4);

    std::thread t([&]() 
    {
        bh.writeFrame3(&frame);
        cv.notify_one();
    });
    t.detach();

    std::chrono::milliseconds timeout(1000);

    {    // blocks when buffer is full
        std::unique_lock<std::mutex> l(m);
        auto r = cv.wait_for(l, timeout);
        REQUIRE( r == std::cv_status::timeout );
    }

    timeout = std::chrono::milliseconds(1);
    
    {   // doesn't block when buffer isn't full
        std::unique_lock<std::mutex> l(m);
        bh.readFrame();
        auto r = cv.wait_for(l, timeout);
        REQUIRE( r == std::cv_status::no_timeout );
    }

}