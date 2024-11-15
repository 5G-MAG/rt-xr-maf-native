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
#include <cmath>

namespace MAF {

extern int componentSize(ComponentType ct)
{
    switch (ct)
    {
        case ComponentType::BYTE:
            return 1;
        case ComponentType::UNSIGNED_BYTE:
            return 1;
        case ComponentType::SHORT:
            return 2;
        case ComponentType::UNSIGNED_SHORT:
            return 2;
        case ComponentType::UNSIGNED_INT:
            return 4;
        case ComponentType::FLOAT:
            return 4;
        default:
            return -1;
    }
}

extern int sampleSize(SampleType st)
{
    switch (st)
    {
        case SampleType::SCALAR:
            return 1;
        case SampleType::VEC2:
            return 2;
        case SampleType::VEC3:
            return 3;
        case SampleType::VEC4:
            return 4;
        case SampleType::MAT2:
            return 4;
        case SampleType::MAT3:
            return 9;
        case SampleType::MAT4:
            return 16;       
        default:
            return -1;
    }
}

extern uint64_t getTimestamp(double time_s){
    // convert seconds elapsed relative to an unspecified epoch, to a frame frame timestamp (ntp 64bit format).
    // ntp timestamps use unsigned int for the integral part of the timestamp
    if (time_s < 0)
        throw "time must be a positive value";
    uint64_t timestamp;
    uint64_t integral = static_cast<uint64_t>(std::truncl(time_s));
    uint64_t fractional = static_cast<uint64_t>((time_s-integral) * 1000000);
    fractional *= 0xFFFFFFFFull;
    fractional /= 1000000;
    timestamp = (integral << 32) | fractional;
    return timestamp;
};

extern uint64_t getTimestamp(int64_t time_us){
    // convert microseconds elapsed relative to an unspecified epoch, to a maf frame timestamp (ntp 64bit format).
    // ntp timestamps use unsigned int for the integral part of the timestamp
    if (time_us < 0)
        throw "time must be a positive value";
    uint64_t integral = time_us * 1000000;
    uint64_t fractional = time_us % 1000000;
    fractional *= 0xFFFFFFFFull;
    fractional /= 1000000;
    return (integral << 32) | fractional;
};

extern double getTime_s(uint64_t timestamp){
    double sec = (double)(timestamp >> 32);
    double usec = (double)(timestamp & 0xFFFFFFFFull) / 0xFFFFFFFFull;
    return sec + usec;
};


Frame::Frame(uint8_t i, uint64_t t, uint32_t byteLength, uint8_t* buffer){
    if (((buffer == nullptr) && (byteLength > 0)) || ((buffer!=nullptr) && (byteLength==0)))
    {
            throw std::invalid_argument("invalid byteLength");
    }
    index = i;
    timestamp = t;
    length = byteLength;
    data = buffer;
}

void BufferHandler::allocate(uint8_t capacity){
    frames_.resize(capacity);
    for (uint8_t i = 0; i < frames_.size(); i++){
        Frame& f = frames_[i];
        f.index = i;
        f.timestamp = 0;
        f.length = 0;
        f.data = nullptr;
    }
    count_ = 0;
    capacity_ = capacity;
    writeIdx_ = 0;
    readIdx_ = 0;
}

void BufferHandler::allocate(uint8_t capacity, uint32_t maxFrameSize){
    frames_.resize(capacity);
    for (uint8_t i = 0; i < frames_.size(); i++){
        Frame& f = frames_[i];
        f.index = i;
        f.timestamp = 0;
        f.length = maxFrameSize;
        f.data = (uint8_t *) malloc(maxFrameSize);
    }
    count_ = 0;
    capacity_ = capacity;
    writeIdx_ = 0;
    readIdx_ = 0;
}

uint8_t BufferHandler::count(){
    return count_;
}

uint8_t BufferHandler::capacity(){
    return capacity_;
}

void BufferHandler::free(){
    for (auto &frame: frames_ ){
        if(frame.data != nullptr){
            std::free(frame.data);
        }
    }
    std::lock_guard<std::mutex> lock(mutex_);
    frames_.clear();
    capacity_ = 0;
    count_ = 0;
}

void BufferHandler::writeFrame(Frame *frame){
    // non-blocking write, skips over readIdx
    // eg. unconstrained/realtime writer, best effort display
    std::lock_guard<std::mutex> lock(mutex_); 

    if(capacity_ == 0)
        throw "buffer handler not allocated";

    if(frame != &frames_[writeIdx_]){
        frames_[writeIdx_].index = writeIdx_;
        frames_[writeIdx_].timestamp = frame->timestamp;
        frames_[writeIdx_].length = frame->length;
        frames_[writeIdx_].data = frame->data;
    };

    if(capacity_ == 1){
        return;
    } 
    
    if (capacity_ == 2 && presentationFrameIdx_ != -1){
        count_ = 1;
        return;
    }

    bool jumps = false;

incrementWriteIdx:
    writeIdx_ = (writeIdx_+1) % capacity_;
    if (writeIdx_ == readIdx_){
        count_ = presentationFrameIdx_ != -1 ? 0 : 1;
        jumps = true;
        goto incrementWriteIdx;
    }
    if(!jumps){
        count_++;
    }
    if (onFrameWrite_ != nullptr){
        (*onFrameWrite_)();
    }

}

void BufferHandler::writeFrame2(Frame *frame){
    // throws when buffer is full
    // eg. writer catches and drops frame
    std::lock_guard<std::mutex> lock(mutex_);
    if(capacity_ == 0)
        throw "buffer handler not allocated";
    if ((readIdx_ == writeIdx_) && (count_ > 0))
        throw "buffer handler is full";
    if(frame != &frames_[writeIdx_]){
        frames_[writeIdx_].index = writeIdx_;
        frames_[writeIdx_].timestamp = frame->timestamp;
        frames_[writeIdx_].length = frame->length;
        frames_[writeIdx_].data = frame->data;
    };
    writeIdx_ = (writeIdx_+1) % capacity_;
    if(count_ < capacity_)
        count_++;
}

void BufferHandler::writeFrame3(Frame* frame){
    // blocks as long as buffer is full
    // eg. frame by frame read without any frame drop
    if ((count_+1) == capacity_){
        std::unique_lock lock(mutex_);
        cvReadFrame_.wait(lock);
    };
    writeFrame2(frame);
}


Frame* BufferHandler::readFrame(){
    std::lock_guard<std::mutex> lock(mutex_);
    if(count_ == 0)
        return nullptr;
    if (presentationFrameIdx_ < 0){
        presentationFrameIdx_ = 0;
        readIdx_ = 0;
    } else {
        readIdx_ = (readIdx_+1) % capacity_;
        presentationFrameIdx_ = readIdx_;
    }
    Frame* frame = &frames_[readIdx_];
    count_--;
    if (onFrameRead_ != nullptr){
        (*onFrameRead_)();
    }
    cvReadFrame_.notify_one();
    return frame;
}


Frame* BufferHandler::readFrame(uint64_t* timestamp){
    if (timestamp == nullptr){
        return readFrame();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if(count_==0){
        return nullptr;
    }
    uint8_t idx = readIdx_;
    while (idx != writeIdx_)
    {
        Frame* frame = &frames_[idx];
        if (frame->timestamp == *timestamp){
            presentationFrameIdx_ = idx;
            return frame;
        }
        idx = (idx+1) % capacity_;
    }
    return nullptr;
}

Frame* BufferHandler::readFrame(double time_s){
    // reads the most recent frame with timestamp >= time_s
    std::lock_guard<std::mutex> lock(mutex_);
    if(count_==0){
        return nullptr;
    }
    int idx = readIdx_;
    Frame* frame = nullptr;
    while (idx != writeIdx_)
    {
        Frame* tmp = &frames_[idx];
        if (getTime_s(tmp ->timestamp) > time_s){
            break;
        }
        frame = tmp;
        idx = (idx+1) % capacity_;
    }
    if (frame != nullptr){
        presentationFrameIdx_ = ((idx + capacity_ )- 1) % capacity_;
    }
    return frame;
}

// void BufferHandler::setOnFrameWriteCallback(void (*onFrameWrite)()){
void BufferHandler::setOnFrameWriteCallback(Callback onFrameWrite){
    std::lock_guard<std::mutex> lock(mutex_);
    onFrameWrite_ = onFrameWrite;
}

// void BufferHandler::setOnFrameReadCallback(void (*onFrameRead)()){
void BufferHandler::setOnFrameReadCallback(Callback onFrameRead){
    std::lock_guard<std::mutex> lock(mutex_);
    onFrameRead_ = onFrameRead;
}

std::vector<Frame>* BufferHandler::getFrames(){
    /* returns an unsafe pointer to the internal frames structure 
        should only be used for buffer initializing, 
            before passing the handler to the MAF pipeline. */
    return &frames_;
}

int BufferHandler::getReadIdx(){
    return readIdx_;
}

int BufferHandler::getWriteIdx(){
    return writeIdx_;
}

}
