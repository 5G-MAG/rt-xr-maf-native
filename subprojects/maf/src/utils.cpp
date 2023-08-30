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
    int sz = 24;
    if (!this->immutable){
        sz += 12;
    }
    sz += (2 * componentSize(this->componentType) * sampleSize(this->type));
    return sz;
}

void BufferInfoHeader::WriteAccessorHeader(uint8_t* data){
    /*
    Writes a frame accessor header at the address pointed to by data.
    Data must be sufficiently allocated. Use GetHeaderSize() to calculate the size of the header. 
    max and min fields must point to valid memory block allocated according to ComponentType and SampleType.
    */
    size_t  p = 0;
    // float
    memcpy(data+p, &this->timestampDelta, 4);
    p += 4;
    if (!this->immutable){
        auto ct = static_cast<uint32_t>(this->componentType);
        memcpy(data+p, &ct, 4);
        p += 4;
        uint8_t st = static_cast<uint8_t>(this->type);
        memcpy(data+p, &st, 1);
        p++;
        // 1 bit + 7 reserved
        memcpy(data+p, &this->normalized, 1);
        p++;
    }
    // uint32_t
    memcpy(data+p, &this->byteOffset, 4);
    p += 4;
    // uint32_t
    memcpy(data+p, &this->count, 4);
    p += 4;
    // max/min
    int itemSize = componentSize(this->componentType) * sampleSize(this->type);
    if (this->max != nullptr){
        memcpy(data+p, this->max, itemSize);
    } else {
        for(int i = 0; i <= itemSize; i++){
            data[p] = 0;   
        }
    }
    p+= itemSize;
    if (this->min != nullptr){
        memcpy(data+p, this->min, itemSize);
    } else {
        for(int i = 0; i <= itemSize; i++){
            data[p] = 0;   
        }
    }
    p+= itemSize;
    // uint32_t
    memcpy(data+p, &this->bufferViewByteOffset, 4);
    p += 4;
    // uint32_t
    memcpy(data+p, &this->bufferViewByteLength, 4);
    p += 4;
    // uint32_t
    memcpy(data+p, &this->bufferViewByteStride, 4);
    p += 4;
}

void BufferInfoHeader::ReadAccessorHeader(uint8_t* data){

    size_t p = 0;
    // float
    memcpy(data, &this->timestampDelta, 4);
    p += 4;
    if (!this->immutable){
        memcpy(&this->componentType, data+p, 4);
        p += 4;
        memcpy(&this->componentType, data+p, 1);
        p++;
        // 1 bit + 7 reserved
        memcpy(&this->normalized, data+p, 1);
        p++;
    }
    // uint32_t
    memcpy(&this->byteOffset, data+p, 4);
    p += 4;
    // uint32_t
    memcpy(&this->count, data+p, 4);
    p += 4;
    // max/min - points to frame data
    int itemSize = getItemSize(this);
    this->max = data+p;
    p += itemSize;
    this->min = data+p;
    p += itemSize;
    // uint32_t
    memcpy(&this->bufferViewByteOffset, data+p, 4);
    p += 4;
    // uint32_t 
    memcpy(&this->bufferViewByteLength, data+p, 4);
    p += 4;
    // uint32_t
    memcpy(&this->bufferViewByteStride, data+p, 4);
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
        h.max = malloc(sz);
        h.min = malloc(sz);
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

