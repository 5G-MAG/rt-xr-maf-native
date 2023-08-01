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
#include <maf.hpp>

using namespace MAF;

TEST_CASE("constructing a Frame", "[MAF_buffer_handler]"){

    uint32_t byteLength = 1920*1080*4;

    SECTION("default destructor doesn't throw"){
        Frame* frame = new Frame();
        REQUIRE_NOTHROW(delete frame);
    }

    SECTION("default constructor doesn't allocate data"){
        Frame* frame = new Frame();
        REQUIRE(frame->index == 0);
        REQUIRE(frame->timestamp== 0);
        REQUIRE(frame->length== 0);
        REQUIRE(frame->data == nullptr);
    }

    SECTION("default constructor sets index and timestamp"){
        Frame* frame = new Frame(9);
        REQUIRE(frame->index == 9);
        REQUIRE(frame->timestamp == 0);
        REQUIRE(frame->length == 0);
        REQUIRE(frame->data == nullptr);
        delete frame;

        frame = new Frame(9, 60000);
        REQUIRE(frame->index == 9);
        REQUIRE(frame->timestamp == 60000);
        REQUIRE(frame->length == 0);
        REQUIRE(frame->data == nullptr);
        delete frame;
    }

    SECTION("specify byteLength, and a buffer doesn't allocate data, data is not owned by the frame"){
        uint8_t * data = new uint8_t[byteLength];
        Frame* frame = new Frame(0, 0, byteLength, data);
        REQUIRE(frame->index == 0);
        REQUIRE(frame->timestamp == 0);
        REQUIRE(frame->length == byteLength);
        REQUIRE(frame->data == data);
        data = frame->data;
        delete frame;
        REQUIRE(data != nullptr);
        REQUIRE_NOTHROW(delete[] data);
    }

}

