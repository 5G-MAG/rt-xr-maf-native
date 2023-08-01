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


#include <iostream>
#include <condition_variable>
#include <queue> 
#include <thread>
#include <atomic>
#include <chrono>
#include <map>

extern "C" {
#ifdef _MSC_VER
    #pragma warning(push) 
    #pragma warning(disable : 4244)
#endif    
    #include <libavcodec/avcodec.h>
    #include <libavcodec/packet.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libavutil/imgutils.h>
#ifdef _MSC_VER
    #pragma warning(default: 4244)
#endif    
}

#include<MafBuffer.hpp>


class Decoder
{
public:
    virtual ~Decoder() = default;
    virtual void QueuePacket(AVPacket* pkt)=0;
    virtual void DecodeInto(MafBuffer* mafBuffer)=0;
    virtual void Start(MafBuffer* mafBuffer)=0;
    virtual void Stop()=0;
    virtual bool IsRunning()=0;
};

Decoder* CreateDecoder(AVFormatContext *fmt_ctx, uint32_t streamIdx);

class VideoDecoder : public Decoder
{
private:
    AVCodecContext *codecCtx_;
    AVStream* stream_;
    std::thread thread_;
    std::exception_ptr error_;
    std::queue<AVPacket*> pktQueue_;
    std::mutex* pktQueueMx_;
    std::mutex* bufferUpdateMx_;
    std::atomic_bool quit_;

public:
    VideoDecoder(AVStream* stream, AVCodecContext* ctx);
    ~VideoDecoder();
    void QueuePacket(AVPacket* pkt);
    void DecodeInto(MafBuffer* mafBuffer);
    void Start(MafBuffer* mafBuffer);
    void Stop();
    bool IsRunning();
};


class AudioDecoder : public Decoder
{
private:
    AVCodecContext *codecCtx_;
    AVStream* stream_;
    std::thread thread_;
    std::exception_ptr error_;
    std::queue<AVPacket*> pktQueue_;
    std::mutex* pktQueueMx_;
    std::mutex* bufferUpdateMx_;
    std::atomic_bool quit_;
    

public:
    AudioDecoder(AVStream* stream, AVCodecContext* ctx);
    ~AudioDecoder();
    void QueuePacket(AVPacket* pkt);
    void DecodeInto(MafBuffer* mafBuffer);
    void Start(MafBuffer* mafBuffer);
    void Stop();
    bool IsRunning();
};
