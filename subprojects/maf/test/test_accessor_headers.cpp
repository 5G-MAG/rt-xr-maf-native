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

TEST_CASE("using BufferInfoHeader", "[MAF_buffer_headers]"){

    SECTION("default constructor"){
        BufferInfoHeader* header = new BufferInfoHeader();
        REQUIRE(header->componentType == ComponentType::UNSIGNED_BYTE);
        REQUIRE(header->type == SampleType::SCALAR);
        REQUIRE(header->count == 0);
        REQUIRE(header->byteOffset == 0);
        REQUIRE(header->bufferViewByteStride == 0);
        REQUIRE(header->bufferViewByteLength == 0);
        REQUIRE(header->bufferViewByteOffset == 0);
        REQUIRE(header->min == nullptr);
        REQUIRE(header->max == nullptr);
        delete header;
    }

    SECTION("can write and read back one immutable VEC4(FLOAT) header"){

        float* max = new float[4]();
        float* min = new float[4](); 
        for (int i=0; i<4; i++){
            max[i] = std::numeric_limits<float>::max()-i;        
            min[i] = std::numeric_limits<float>::min()+i;        
        }
            
        BufferInfoHeader* header = new BufferInfoHeader();
        header->componentType = ComponentType::FLOAT;
        header->type = SampleType::VEC4;
        header->count = 123456;
        header->byteOffset = 567890;
        header->bufferViewByteStride = 14095;
        header->bufferViewByteLength = 24096;
        header->bufferViewByteOffset = 34097;
        header->max = (void *)max;
        header->min = (void *)min;
        int sz = header->GetHeaderSize();
        uint8_t* buff = new uint8_t[sz]();
        header->WriteAccessorHeader(buff);
        delete header;
        delete[] max;
        delete[] min;

        header = new BufferInfoHeader();
        header->componentType = ComponentType::FLOAT;
        header->type = SampleType::VEC4;
        header->ReadAccessorHeader(buff);
        REQUIRE(header->componentType == ComponentType::FLOAT);
        REQUIRE(header->type == SampleType::VEC4);
        REQUIRE(header->count == 123456);
        REQUIRE(header->byteOffset == 567890);
        REQUIRE(header->bufferViewByteStride == 14095);
        REQUIRE(header->bufferViewByteLength == 24096);
        REQUIRE(header->bufferViewByteOffset == 34097);
        REQUIRE(header->min != nullptr);
        REQUIRE(header->max != nullptr);
        max = (float *)(header->max);
        min = (float *)(header->min);
        for (int i=0; i<4; i++){
            REQUIRE(max[i] == std::numeric_limits<float>::max()-i);
            REQUIRE(min[i] == std::numeric_limits<float>::min()+i);
        }
        delete[] buff;
        delete header;
    }


    SECTION("can write and read back one immutable SCALAR(FLOAT) header"){

        float max = std::numeric_limits<float>::max();
        float min = std::numeric_limits<float>::min();
            
        BufferInfoHeader* header = new BufferInfoHeader();
        header->componentType = ComponentType::FLOAT;
        header->type = SampleType::SCALAR;
        header->count = 123456;
        header->byteOffset = 567890;
        header->bufferViewByteStride = 14095;
        header->bufferViewByteLength = 24096;
        header->bufferViewByteOffset = 34097;
        header->max = (void *)&max;
        header->min = (void *)&min;

        uint8_t* buff = new uint8_t[header->GetHeaderSize()]();
        header->WriteAccessorHeader(buff);
        delete header;

        header = new BufferInfoHeader();
        header->componentType = ComponentType::FLOAT;
        header->type = SampleType::SCALAR;
        header->ReadAccessorHeader(buff);
        REQUIRE(header->componentType == ComponentType::FLOAT);
        REQUIRE(header->type == SampleType::SCALAR);
        REQUIRE(header->count == 123456);
        REQUIRE(header->byteOffset == 567890);
        REQUIRE(header->bufferViewByteStride == 14095);
        REQUIRE(header->bufferViewByteLength == 24096);
        REQUIRE(header->bufferViewByteOffset == 34097);
        REQUIRE(header->max != nullptr);
        REQUIRE(max == *(float *)header->max);
        REQUIRE(header->min != nullptr);
        REQUIRE(min == *(float *)header->min);
        delete header;

        delete[] buff;

    }


    SECTION("can write and read back an array of immutable headers"){

        float max = std::numeric_limits<float>::max();
        float min = std::numeric_limits<float>::min();
        std::vector<BufferInfo>* bufferInfo = new std::vector<BufferInfo>(1);
        (*bufferInfo)[0].componentType = ComponentType::FLOAT;
        (*bufferInfo)[0].type = SampleType::SCALAR;
        
        std::vector<BufferInfoHeader>* headers = BufferInfoHeader::CreateAccessorsHeaders(bufferInfo, true);
        BufferInfoHeader* header = &(*headers)[0];
        REQUIRE(header->componentType == ComponentType::FLOAT);
        REQUIRE(header->type == SampleType::SCALAR);        
        REQUIRE(header->immutable);
        header->count = 123456;
        header->byteOffset = 567890;
        header->bufferViewByteStride = 14095;
        header->bufferViewByteLength = 24096;
        header->bufferViewByteOffset = 34097;
        header->max = (void *)&max;
        header->min = (void *)&min;

        int sz = (*headers)[0].GetHeaderSize();
        uint8_t* buff = new uint8_t[sz]();
        BufferInfoHeader::WriteAccessorsHeaders(buff, headers);
        delete headers;

        headers = BufferInfoHeader::CreateAccessorsHeaders(bufferInfo, true);
        BufferInfoHeader::ReadAccessorsHeaders(buff, headers);
        header = &(*headers)[0];
        REQUIRE(header->componentType == ComponentType::FLOAT);
        REQUIRE(header->type == SampleType::SCALAR);
        REQUIRE(header->count == 123456);
        REQUIRE(header->byteOffset == 567890);
        REQUIRE(header->bufferViewByteStride == 14095);
        REQUIRE(header->bufferViewByteLength == 24096);
        REQUIRE(header->bufferViewByteOffset == 34097);
        REQUIRE(header->max != nullptr);
        REQUIRE(max == *(float *)header->max);
        REQUIRE(header->min != nullptr);
        REQUIRE(min == *(float *)header->min);
        delete header;

        delete[] buff;

    }
}

