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


#include <cstring>
#include <cstdlib>
#include <maf.hpp>

using namespace MAF;

inline int getItemSize(BufferInfoHeader* hdr){
    return sampleSize(hdr->type) * componentSize(hdr->componentType);
}

inline int getItemSize(SampleType type, ComponentType componentType){
    return sampleSize(type) * componentSize(componentType);
}


/* public functions ************************************************************************/ 

int BufferInfoHeader::GetHeaderSize(){
    // Table 8: timestamp_delta (4), byteOffset (4), count (4), bufferViewByteOffset, Length and Stride (12),
    // then max and min; a mutable header adds componentType (4), type (1) and normalized with its reserved bits (1)
    int sz = 24;
    if (!this->immutable){
        sz += 6;
    }
    sz += (2 * componentSize(this->componentType) * sampleSize(this->type));
    return sz;
}

// 5.1.2: "uint(n) ... interpreted as a binary representation of an unsigned integer with the most significant bit written first",
// float(n) as IEEE 754: the first byte of a multi-byte field is the most significant one (big-endian). The first bit of the byte that
// follows the type field is the normalized bit (Table 8: normalized u(1), reserved_zero_bit u(7)).
static void putBE32(uint8_t* d, uint32_t v){ d[0] = v >> 24; d[1] = v >> 16; d[2] = v >> 8; d[3] = v; }
static uint32_t getBE32(const uint8_t* d){ return ((uint32_t)d[0] << 24) | ((uint32_t)d[1] << 16) | ((uint32_t)d[2] << 8) | d[3]; }

void BufferInfoHeader::WriteAccessorHeader(uint8_t* data){
    /*
    Writes a frame accessor header at the address pointed to by data.
    Data must be sufficiently allocated. Use GetHeaderSize() to calculate the size of the header. 
    max and min fields must point to valid memory block allocated according to ComponentType and SampleType.
    */
    size_t  p = 0;
    // float
    uint32_t tsBits; memcpy(&tsBits, &this->timestampDelta, 4);
    putBE32(data+p, tsBits);
    p += 4;
    if (!this->immutable){
        auto ct = static_cast<uint32_t>(this->componentType);
        putBE32(data+p, ct);
        p += 4;
        uint8_t st = static_cast<uint8_t>(this->type);
        memcpy(data+p, &st, 1);
        p++;
        // 1 bit + 7 reserved bits equal to 0
        uint8_t norm = this->normalized ? 0x80 : 0;
        memcpy(data+p, &norm, 1);
        p++;
    }
    // uint32_t
    putBE32(data+p, this->byteOffset);
    p += 4;
    // uint32_t
    putBE32(data+p, this->count);
    p += 4;
    // max/min
    int itemSize = componentSize(this->componentType) * sampleSize(this->type);
    if (this->max != nullptr){
        memcpy(data+p, this->max, itemSize);
    } else {
        memset(data+p, 0, itemSize);
    }
    p+= itemSize;
    if (this->min != nullptr){
        memcpy(data+p, this->min, itemSize);
    } else {
        memset(data+p, 0, itemSize);
    }
    p+= itemSize;
    // uint32_t
    putBE32(data+p, this->bufferViewByteOffset);
    p += 4;
    // uint32_t
    putBE32(data+p, this->bufferViewByteLength);
    p += 4;
    // uint32_t
    putBE32(data+p, this->bufferViewByteStride);
    p += 4;
}

void BufferInfoHeader::ReadAccessorHeader(uint8_t* data){

    size_t p = 0;
    // float
    uint32_t tsBits = getBE32(data);
    memcpy(&this->timestampDelta, &tsBits, 4);
    p += 4;
    if (!this->immutable){
        uint32_t ct = getBE32(data+p);
        this->componentType = static_cast<ComponentType>(ct);
        p += 4;
        uint8_t st = 0;
        memcpy(&st, data+p, 1);
        this->type = static_cast<SampleType>(st);
        p++;
        // 1 bit + 7 reserved
        uint8_t norm = 0;
        memcpy(&norm, data+p, 1);
        this->normalized = (norm & 0x80) != 0;
        p++;
    }
    // uint32_t
    this->byteOffset = getBE32(data+p);
    p += 4;
    // uint32_t
    this->count = getBE32(data+p);
    p += 4;
    // max/min - points to frame data
    int itemSize = getItemSize(this);
    this->max = data+p;
    p += itemSize;
    this->min = data+p;
    p += itemSize;
    // uint32_t
    this->bufferViewByteOffset = getBE32(data+p);
    p += 4;
    // uint32_t
    this->bufferViewByteLength = getBE32(data+p);
    p += 4;
    // uint32_t
    this->bufferViewByteStride = getBE32(data+p);
    p += 4;
}


/* Static functions ************************************************************************/ 

std::vector<BufferInfoHeader>* BufferInfoHeader::CreateAccessorsHeaders(std::vector<BufferInfo>* bufferInfo, bool immutable){
    // create a vector of headers that match the buffer info in MafBuffer
    std::vector<BufferInfoHeader>* headers = new std::vector<BufferInfoHeader>(bufferInfo->size());
    int i = 0;
    for (auto &h : *headers){
        h.type = (*bufferInfo)[i].type;
        h.componentType = (*bufferInfo)[i].componentType;
        h.immutable = immutable;
        auto sz = getItemSize(&h);
        h.max = calloc(1, sz);
        h.min = calloc(1, sz);
        i++;
    }
    return headers;
}

void BufferInfoHeader::ComputeTotalHeadersLength(std::vector<BufferInfoHeader>* headers, int &bytelength){
    // keep in mind that mutable accessors imply that the result may vary from frame to frame
    int res = 0;
    for (auto &h : *headers){
        res += h.GetHeaderSize();
    }
    bytelength = res;
}

void BufferInfoHeader::ComputeAccessorsHeadersInterleaved(std::vector<BufferInfoHeader>* headers){
    /*
    * computes each header's byteOffset and bufferViewByte* properties,
    * assuming headers are written contiguously, and the referenced media data is interleaved in the buffer immediately following headers.
    */
    int bufferViewByteLength = 0;
    int bufferViewByteOffset = 0;
    int accessorOffset = 0;
    for (auto &h : *headers){
        h.byteOffset = accessorOffset;
        int itemSize = getItemSize(&h);
        bufferViewByteLength += itemSize * h.count;
        bufferViewByteOffset += h.GetHeaderSize();
        accessorOffset += itemSize;
    }
    for (auto &h : *headers){
        h.bufferViewByteLength = bufferViewByteLength;
        h.bufferViewByteOffset = bufferViewByteOffset;
        h.bufferViewByteStride = accessorOffset;
    }
}

void BufferInfoHeader::ComputeAccessorsHeadersPlanar(std::vector<BufferInfoHeader>* headers){
    /*
    * computes each header's byteOffset and bufferViewByte* properties,
    * assuming headers are written contiguously, and the referenced media data is stored in contiguous data planes immediately following headers.
    */
    int bufferViewByteOffset = 0;
    int accessorOffset = 0;
    for (auto &h : *headers){
        h.byteOffset = accessorOffset;
        accessorOffset += getItemSize(&h) * h.count;
        bufferViewByteOffset += h.GetHeaderSize();
    }
    for (auto &h : *headers){
        h.bufferViewByteLength = accessorOffset;
        h.bufferViewByteOffset = bufferViewByteOffset;
        h.bufferViewByteStride = 0;
    }
}

void BufferInfoHeader::WriteAccessorsHeaders(uint8_t* data, std::vector<BufferInfoHeader>* headers){
    for (auto &h : *headers){
        h.WriteAccessorHeader(data);
        data += h.GetHeaderSize();
    }
}

void BufferInfoHeader::ReadAccessorsHeaders(uint8_t* data, std::vector<BufferInfoHeader>* headers){
    for (auto &h : *headers){
        h.ReadAccessorHeader(data);
        data += h.GetHeaderSize();
    }
    
}

