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

#pragma once

#include <iostream>
#include <condition_variable>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include <map>

extern "C"
{
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
#pragma warning(default : 4244)
#endif
}

#include <MafBuffer.hpp>

// Whether the buffer of these accessors has a timed accessor information header (ISO/IEC 23090-14, Table 8) before the
// data of each frame.
//
// IMPLEMENTATION CONVENTION, NOT PART OF ISO/IEC 23090-14. 5.2.2.3 has the presentation engine pass the header
// configuration to the MAF, but the MAF API of clause 6 has no field to carry it, so this implementation infers it
// from the buffer layout:
// - the header is assumed present when the offset of the first accessor is at least the summed header sizes of all
//   the accessors of the buffer (the layout produced when the header bufferView precedes the data in the glTF);
// - the header is always written with the immutable layout, whatever the accessor's MPEG_accessor_timed.immutable.
// A conforming presentation engine may lay out its buffers differently: data placed at a non-zero offset for another
// reason is mistaken for a header, and a mutable header is written with the wrong layout. Replace this inference
// with an explicit field once the MAF API carries the header configuration.
bool HasTimedAccessorHeader(const std::vector<MAF::BufferInfo> &infos);

// Input of a decoder thread: compressed packets in decoding order, and markers placed between them.
// A LOOP marker makes the decoder output every frame it still holds, then continue with a new timestamp base;
// an END marker makes it output every frame it still holds, then stop.
struct DecoderQueueItem
{
    static const int PACKET = 0;
    static const int LOOP = 1;
    static const int END = 2;
    AVPacket *pkt = nullptr;
    int kind = PACKET;
    double base = 0.0; // LOOP: timestamp, in seconds, of the start of the next pass
};

class DecoderQueue
{
private:
    std::queue<DecoderQueueItem> items_;
    std::mutex mutex_;

public:
    ~DecoderQueue();
    void PushPacket(AVPacket *pkt); // takes the content of pkt
    void PushMarker(int kind, double base);
    bool Front(DecoderQueueItem &item);
    void Pop();
    size_t Size();
};

class Decoder
{
public:
    virtual ~Decoder() = default;
    virtual void QueuePacket(AVPacket *pkt) = 0;
    virtual void DecodeInto(MafBuffer *mafBuffer) = 0;
    virtual void Start(MafBuffer *mafBuffer) = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() = 0;
    // Frames before startSource_s (when trimStart is true) and from endSource_s on (when it is not negative) are dropped.
    // Times are source times in seconds. Must be called before Start().
    virtual void SetRange(double startSource_s, double endSource_s, bool trimStart) = 0;
    virtual void QueueLoop(double newBase_s) = 0;
    virtual void QueueEnd() = 0;
    // true once the decoder has output everything up to the END marker, or has failed
    virtual bool IsDone() = 0;
    virtual bool HasError() = 0;
    virtual size_t QueuedPackets() = 0;
};

Decoder *CreateDecoder(AVFormatContext *fmt_ctx, uint32_t streamIdx);

class VideoDecoder : public Decoder
{
private:
    AVCodecContext *codecCtx_;
    AVStream *stream_;
    std::thread thread_;
    std::exception_ptr error_;
    DecoderQueue queue_;
    std::mutex *bufferUpdateMx_;
    std::atomic_bool quit_;
    std::atomic_bool done_{false};
    std::atomic_bool failed_{false};
    double rangeStart_ = 0.0;
    double rangeEnd_ = -1.0;
    bool trimStart_ = false;
    uint32_t maxFrameBytes_ = 0;

public:
    VideoDecoder(AVStream *stream, AVCodecContext *ctx);
    ~VideoDecoder();
    void QueuePacket(AVPacket *pkt);
    void DecodeInto(MafBuffer *mafBuffer);
    void Start(MafBuffer *mafBuffer);
    void Stop();
    bool IsRunning();
    void SetRange(double startSource_s, double endSource_s, bool trimStart);
    void QueueLoop(double newBase_s);
    void QueueEnd();
    bool IsDone();
    bool HasError();
    size_t QueuedPackets();
};

class AudioDecoder : public Decoder
{
private:
    AVCodecContext *codecCtx_;
    AVStream *stream_;
    std::thread thread_;
    std::exception_ptr error_;
    DecoderQueue queue_;
    std::mutex *bufferUpdateMx_;
    std::atomic_bool quit_;
    std::atomic_bool done_{false};
    std::atomic_bool failed_{false};
    double rangeStart_ = 0.0;
    double rangeEnd_ = -1.0;
    bool trimStart_ = false;
    uint32_t maxFrameBytes_ = 0;

public:
    AudioDecoder(AVStream *stream, AVCodecContext *ctx);
    ~AudioDecoder();
    void QueuePacket(AVPacket *pkt);
    void DecodeInto(MafBuffer *mafBuffer);
    void Start(MafBuffer *mafBuffer);
    void Stop();
    bool IsRunning();
    void SetRange(double startSource_s, double endSource_s, bool trimStart);
    void QueueLoop(double newBase_s);
    void QueueEnd();
    bool IsDone();
    bool HasError();
    size_t QueuedPackets();
};
