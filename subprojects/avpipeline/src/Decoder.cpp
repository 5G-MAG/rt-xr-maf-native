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
#include <chrono>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cstring>
#ifdef __ANDROID__
#include <cstdio>
#include <cstdarg>
static void decLog(const char *fmt, ...)
{
    FILE *f = fopen("/sdcard/Android/data/com.fivegmag.rtxrplayer/files/avpipeline.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap); fputc('\n', f); fclose(f);
}
#else
#define decLog(...) do {} while (0)
#endif

#include <vector>

extern "C"
{
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4244)
#endif

#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#ifdef _MSC_VER
    #pragma warning(default : 4244)
#endif
}

#include <Decoder.hpp>
#include <MetadataDecoder.hpp>

// Reformat the decoded audio to the buffer layout: sample format, channel layout (planar/interleaved) and,
// when BufferInfo.targetSampleRate is set, the sample rate.
#define ENABLE_AUDIO_RESAMPLING 1

using ms = std::chrono::duration<double, std::milli>;


static void idleSleep()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

static Frame* getWritableFrame(BufferHandler* handler){
    // returns the frame at writeIdx unless it is the current presentation frame
    if (handler->capacity() < 2)
        return nullptr;
    if ((handler->getWriteIdx() == handler->getReadIdx()) && (handler->count() > 0))
        return nullptr;
    return &handler->getFrames()->at(handler->getWriteIdx());
}


DecoderQueue::~DecoderQueue()
{
    std::lock_guard<std::mutex> lock(mutex_);
    while (!items_.empty())
    {
        DecoderQueueItem item = items_.front();
        items_.pop();
        if (item.pkt)
        {
            av_packet_free(&item.pkt);
        }
    }
}

void DecoderQueue::PushPacket(AVPacket *pkt)
{
    AVPacket *p = av_packet_alloc();
    if (!p)
    {
        av_packet_unref(pkt);
        throw "failed to queue packet";
    }
    av_packet_move_ref(p, pkt);
    DecoderQueueItem item;
    item.pkt = p;
    std::lock_guard<std::mutex> lock(mutex_);
    items_.push(item);
}

void DecoderQueue::PushMarker(int kind, double base)
{
    DecoderQueueItem item;
    item.kind = kind;
    item.base = base;
    std::lock_guard<std::mutex> lock(mutex_);
    items_.push(item);
}

bool DecoderQueue::Front(DecoderQueueItem &item)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (items_.empty())
    {
        return false;
    }
    item = items_.front();
    return true;
}

void DecoderQueue::Pop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!items_.empty())
    {
        items_.pop();
    }
}

size_t DecoderQueue::Size()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return items_.size();
}


Decoder *CreateDecoder(AVFormatContext *fmtCtx, uint32_t streamIdx)
{

    const AVCodec *codec;
    AVCodecContext *codecCtx;
    AVStream *stream;
    int err = 0;
    if (streamIdx >= fmtCtx->nb_streams)
    {
        throw "invalid stream id";
    }
    stream = fmtCtx->streams[streamIdx];
    if (stream->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_DATA)
    {
        // timed metadata tracks are not decoded (ISO/IEC 23090-14:2024, clause 7)
        return CreateMetadataDecoder(fmtCtx, streamIdx);
    }
    codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec)
    {
        throw AVERROR(EINVAL);
    }
    codecCtx = avcodec_alloc_context3(codec);

    if (!codecCtx)
    {
        throw AVERROR(ENOMEM);
    }

    avcodec_parameters_to_context(codecCtx, stream->codecpar);

    if ((err = avcodec_open2(codecCtx, codec, nullptr)) < 0)
    {
        throw err;
    }

    if (codec->type == AVMediaType::AVMEDIA_TYPE_VIDEO)
        return new VideoDecoder(stream, codecCtx);

    if (codec->type == AVMediaType::AVMEDIA_TYPE_AUDIO)
        return new AudioDecoder(stream, codecCtx);

    return nullptr;
}

/*******************************************************************************************************************************************/

namespace
{

    enum class ChannelExtract
    {
        NONE,  // the converted picture is stored as is
        RED,   // the converted picture is RGB24, one component is stored
        GREEN,
        BLUE,
        RG
    };

    struct VideoOutputFormat
    {
        AVPixelFormat swsFormat;  // pixel format produced by the converter
        int swsBytesPerPixel;
        int bytesPerPixel;        // bytes per pixel stored in the buffer frame
        ChannelExtract extract;
    };

    VideoOutputFormat getVideoOutputFormat(AttributeType attributeType)
    {
        switch (attributeType)
        {
        case AttributeType::TEXTURE_BGR:
            return {AV_PIX_FMT_BGR24, 3, 3, ChannelExtract::NONE};
        case AttributeType::TEXTURE_RGBA:
            return {AV_PIX_FMT_RGBA, 4, 4, ChannelExtract::NONE};
        case AttributeType::TEXTURE_BGRA:
            return {AV_PIX_FMT_BGRA, 4, 4, ChannelExtract::NONE};
        case AttributeType::TEXTURE_RED:
            return {AV_PIX_FMT_RGB24, 3, 1, ChannelExtract::RED};
        case AttributeType::TEXTURE_GREEN:
            return {AV_PIX_FMT_RGB24, 3, 1, ChannelExtract::GREEN};
        case AttributeType::TEXTURE_BLUE:
            return {AV_PIX_FMT_RGB24, 3, 1, ChannelExtract::BLUE};
        case AttributeType::TEXTURE_RG:
            return {AV_PIX_FMT_RGB24, 3, 2, ChannelExtract::RG};
        case AttributeType::TEXTURE_DEPTH_COMPONENT:
            // a single component taken from the luma of the decoded picture
            return {AV_PIX_FMT_GRAY8, 1, 1, ChannelExtract::NONE};
        default:
            return {AV_PIX_FMT_RGB24, 3, 3, ChannelExtract::NONE};
        }
    }

    // Size at which a decoded picture is stored: the decoded size, reduced (keeping the aspect ratio) when it exceeds
    // the maximum picture size of the texture or does not fit in the buffer frame.
    void fitPictureSize(int w, int h, uint32_t maxW, uint32_t maxH, uint32_t capacityBytes, int bytesPerPixel, int &outW, int &outH)
    {
        double scale = 1.0;
        if (maxW > 0 && (uint32_t)w > maxW)
        {
            scale = std::min(scale, (double)maxW / (double)w);
        }
        if (maxH > 0 && (uint32_t)h > maxH)
        {
            scale = std::min(scale, (double)maxH / (double)h);
        }
        double bytes = (double)w * (double)h * (double)bytesPerPixel;
        if (bytes * scale * scale > (double)capacityBytes)
        {
            scale = std::min(scale, std::sqrt((double)capacityBytes / bytes));
        }
        if (scale >= 1.0)
        {
            outW = w;
            outH = h;
            return;
        }
        // widths that are a multiple of 4 keep every row aligned for the texture upload
        outW = std::max(4, ((int)std::floor((double)w * scale)) & ~3);
        outH = std::max(1, (int)std::floor((double)h * scale));
    }

} // namespace

VideoDecoder::VideoDecoder(AVStream *stream, AVCodecContext *ctx)
{
    stream_ = stream;
    codecCtx_ = ctx;
    bufferUpdateMx_ = new std::mutex();
}

VideoDecoder::~VideoDecoder()
{
    Stop();
    if (codecCtx_)
    {
        avcodec_free_context(&codecCtx_);
    }

    delete bufferUpdateMx_;
}

void VideoDecoder::QueuePacket(AVPacket *pkt)
{
    queue_.PushPacket(pkt);
}

void VideoDecoder::SetRange(double startSource_s, double endSource_s, bool trimStart)
{
    rangeStart_ = startSource_s;
    rangeEnd_ = endSource_s;
    trimStart_ = trimStart;
}

void VideoDecoder::QueueLoop(double newBase_s)
{
    queue_.PushMarker(DecoderQueueItem::LOOP, newBase_s);
}

void VideoDecoder::QueueEnd()
{
    queue_.PushMarker(DecoderQueueItem::END, 0.0);
}

bool VideoDecoder::IsDone()
{
    return done_;
}

bool VideoDecoder::HasError()
{
    return failed_;
}

size_t VideoDecoder::QueuedPackets()
{
    return queue_.Size();
}

void VideoDecoder::DecodeInto(MafBuffer *mafBuffer)
{
    BufferHandler* handler = mafBuffer->first;
    const BufferInfo info = mafBuffer->second.at(0);

    // decoding
    AVFrame *avFrame = av_frame_alloc();

    // conversion
    const VideoOutputFormat outFmt = getVideoOutputFormat(info.attributeType);
    struct SwsContext *swsCtx = nullptr;
    std::vector<uint8_t> converted;

    const double timeBase = av_q2d(stream_->time_base);
    double frameDuration = 0.04;
    if (stream_->avg_frame_rate.num > 0 && stream_->avg_frame_rate.den > 0)
    {
        frameDuration = 1.0 / av_q2d(stream_->avg_frame_rate);
    }

    try
    {
        if (info.componentType != ComponentType::UNSIGNED_BYTE)
        {
            throw "video buffers require an accessor with unsigned byte components";
        }

        // layout of a buffer frame: the timed accessor information header when requested, then the picture
        BufferInfoHeader header;
        int headerSize = 0;
        if (info.timedHeader)
        {
            header.immutable = info.immutable;
            header.componentType = info.componentType;
            header.type = info.type;
            header.normalized = info.normalized;
            headerSize = header.GetHeaderSize();
        }
        const uint32_t dataOffset = info.timedHeader ? (uint32_t)headerSize : (info.offset > 0 ? (uint32_t)info.offset : 0);
        if (maxFrameBytes_ <= dataOffset)
        {
            throw "buffer frame is too small for a picture";
        }
        const uint32_t dataCapacity = maxFrameBytes_ - dataOffset;

        DecoderQueueItem pending;
        bool draining = false;
        double base = 0.0;
        double lastSourceTime = -1.0;

        while (!quit_)
        {
            bool progress = false;

            if (!draining)
            {
                DecoderQueueItem item;
                if (queue_.Front(item))
                {
                    if (item.kind == DecoderQueueItem::PACKET)
                    {
                        int err = avcodec_send_packet(codecCtx_, item.pkt);
                        if (err >= 0)
                        {
                            queue_.Pop();
                            av_packet_free(&item.pkt);
                            progress = true;
                        }
                        else if (err != AVERROR(EAGAIN))
                        {
                            queue_.Pop();
                            av_packet_free(&item.pkt);
                            throw err;
                        }
                    }
                    else
                    {
                        // output the frames still held by the decoder
                        int err = avcodec_send_packet(codecCtx_, nullptr);
                        if (err >= 0 || err == AVERROR_EOF)
                        {
                            queue_.Pop();
                            pending = item;
                            draining = true;
                            progress = true;
                        }
                        else if (err != AVERROR(EAGAIN))
                        {
                            throw err;
                        }
                    }
                }
            }

            Frame* mafFrame = getWritableFrame(handler);
            if (mafFrame != nullptr)
            {
                int err1 = avcodec_receive_frame(codecCtx_, avFrame);
                if (err1 == 0)
                {
                    progress = true;

                    int64_t pts = avFrame->pts != AV_NOPTS_VALUE ? avFrame->pts : avFrame->best_effort_timestamp;
                    double sourceTime = (pts != AV_NOPTS_VALUE) ? pts * timeBase : (lastSourceTime >= 0.0 ? lastSourceTime + frameDuration : rangeStart_);
                    lastSourceTime = sourceTime;

                    bool dropped = (trimStart_ && sourceTime < rangeStart_ - 1e-6) || (rangeEnd_ >= 0.0 && sourceTime >= rangeEnd_);
                    if (dropped)
                    {
                        av_frame_unref(avFrame);
                    }
                    else
                    {
                        const int srcW = avFrame->width;
                        const int srcH = avFrame->height;
                        int outW = srcW;
                        int outH = srcH;
                        fitPictureSize(srcW, srcH, info.maxWidth, info.maxHeight, dataCapacity, outFmt.bytesPerPixel, outW, outH);

                        int flags = (outW != srcW || outH != srcH) ? SWS_BILINEAR : (SWS_FAST_BILINEAR | SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND);
                        swsCtx = sws_getCachedContext(swsCtx, srcW, srcH, (enum AVPixelFormat)avFrame->format,
                                                      outW, outH, outFmt.swsFormat, flags, NULL, NULL, NULL);
                        if (!swsCtx)
                        {
                            throw "failed to create the picture converter";
                        }

                        // converter output goes through a padded intermediate buffer
                        const size_t convertedBytes = (size_t)outW * (size_t)outH * (size_t)outFmt.swsBytesPerPixel;
                        converted.resize(convertedBytes + 64);
                        uint8_t *dstPlanes[4] = {converted.data(), nullptr, nullptr, nullptr};
                        int dstStride[4] = {outW * outFmt.swsBytesPerPixel, 0, 0, 0};
                        sws_scale(swsCtx, avFrame->data, avFrame->linesize, 0, srcH, dstPlanes, dstStride);

                        uint8_t *pData = mafFrame->data + dataOffset;
                        const size_t pixels = (size_t)outW * (size_t)outH;
                        switch (outFmt.extract)
                        {
                        case ChannelExtract::RED:
                        case ChannelExtract::GREEN:
                        case ChannelExtract::BLUE:
                        {
                            int c = outFmt.extract == ChannelExtract::RED ? 0 : (outFmt.extract == ChannelExtract::GREEN ? 1 : 2);
                            for (size_t i = 0; i < pixels; i++)
                            {
                                pData[i] = converted[i * 3 + c];
                            }
                            break;
                        }
                        case ChannelExtract::RG:
                            for (size_t i = 0; i < pixels; i++)
                            {
                                pData[i * 2] = converted[i * 3];
                                pData[i * 2 + 1] = converted[i * 3 + 1];
                            }
                            break;
                        default:
                            memcpy(pData, converted.data(), pixels * (size_t)outFmt.bytesPerPixel);
                            break;
                        }

                        const uint32_t dataBytes = (uint32_t)(pixels * (size_t)outFmt.bytesPerPixel);
                        if (info.timedHeader)
                        {
                            header.timestampDelta = 0.0f;
                            header.byteOffset = 0;
                            header.count = (uint32_t)pixels;
                            header.bufferViewByteOffset = (uint32_t)headerSize;
                            header.bufferViewByteLength = dataBytes;
                            header.bufferViewByteStride = 0;
                            header.WriteAccessorHeader(mafFrame->data);
                        }

                        double timestamp = base + std::max(0.0, sourceTime - rangeStart_);
                        mafFrame->timestamp = getTimestamp(timestamp);
                        mafFrame->length = dataOffset + dataBytes;
                        mafFrame->extraFrameInfoFlags = 0x01;
                        mafFrame->width = (uint32_t)outW;
                        mafFrame->height = (uint32_t)outH;
                        av_frame_unref(avFrame);

                        while (!quit_){
                            try {
                                handler->writeFrame2(mafFrame);
                                break;
                            } catch (...){
                                idleSleep();
                                continue;
                            }
                        }
                    }
                }
                else if (err1 == AVERROR_EOF)
                {
                    // everything before the marker has been output
                    avcodec_flush_buffers(codecCtx_);
                    draining = false;
                    progress = true;
                    if (pending.kind == DecoderQueueItem::LOOP)
                    {
                        base = pending.base;
                        lastSourceTime = -1.0;
                    }
                    else
                    {
                        break;
                    }
                }
                else if (err1 != AVERROR(EAGAIN))
                {
                    throw err1;
                }
            }

            if (!progress)
            {
                idleSleep();
            }
        }
    }
    catch (const int &ex)
    {
        char errText[128]; av_strerror(ex, errText, sizeof errText);
        decLog("decoder error: %d (%s)", ex, errText);
        error_ = make_exception_ptr(std::runtime_error(std::to_string(ex)));
        failed_ = true;
    }
    catch (const char *ex)
    {
        decLog("decoder error: %s", ex);
        error_ = make_exception_ptr(std::runtime_error(std::string(ex)));
        failed_ = true;
    }
    catch (...)
    {
        error_ = std::current_exception();
        failed_ = true;
    }

    av_frame_free(&avFrame);
    if (swsCtx)
        sws_freeContext(swsCtx);
    done_ = true;
}

void VideoDecoder::Start(MafBuffer *mafBuffer)
{
    if (IsRunning())
    {
        return;
    }
    quit_ = false;
    // every frame of a buffer is allocated with the same size; DecodeInto() later shortens Frame.length to the data written
    if (maxFrameBytes_ == 0 && !mafBuffer->first->getFrames()->empty())
    {
        maxFrameBytes_ = mafBuffer->first->getFrames()->at(0).length;
    }
    thread_ = std::thread(&VideoDecoder::DecodeInto, this, mafBuffer);
}

void VideoDecoder::Stop()
{
    quit_ = true;
    if (IsRunning())
    {
        thread_.join();
    }
}

bool VideoDecoder::IsRunning()
{
    return thread_.joinable();
}

/*******************************************************************************************************************************************/

AudioDecoder::AudioDecoder(AVStream* stream, AVCodecContext *ctx)
{
    stream_ = stream;
    codecCtx_ = ctx;
    bufferUpdateMx_ = new std::mutex();
}

AudioDecoder::~AudioDecoder()
{
    Stop();
    if (codecCtx_)
    {
        avcodec_free_context(&codecCtx_);
    }

    delete bufferUpdateMx_;
}

void AudioDecoder::QueuePacket(AVPacket *pkt)
{
    // TODO: drop late packets
    queue_.PushPacket(pkt);
}

void AudioDecoder::SetRange(double startSource_s, double endSource_s, bool trimStart)
{
    rangeStart_ = startSource_s;
    rangeEnd_ = endSource_s;
    trimStart_ = trimStart;
}

void AudioDecoder::QueueLoop(double newBase_s)
{
    queue_.PushMarker(DecoderQueueItem::LOOP, newBase_s);
}

void AudioDecoder::QueueEnd()
{
    queue_.PushMarker(DecoderQueueItem::END, 0.0);
}

bool AudioDecoder::IsDone()
{
    return done_;
}

bool AudioDecoder::HasError()
{
    return failed_;
}

size_t AudioDecoder::QueuedPackets()
{
    return queue_.Size();
}

inline void copyAudioSamples(uint8_t ** src, int srcSampleOffset, uint8_t * dst, int dstSampleOffset, int numSamples, int sampleSize, int numChannels, bool packedChannels)
{
    int chanByteLength = sampleSize * numSamples;
    size_t srcByteOffset = srcSampleOffset * sampleSize;
    size_t dstByteOffset = dstSampleOffset * sampleSize;
    if(numChannels == 1){
        memcpy(dst + dstByteOffset, src[0] + srcByteOffset, chanByteLength);
    }
    else if (packedChannels)
    {
        srcByteOffset *= numChannels;
        dstByteOffset *= numChannels;
        memcpy(dst + dstByteOffset, src[0] + srcByteOffset, chanByteLength * numChannels);
    }
    else
    {
        for (int ch = 0; ch < numChannels; ch++)
        {
            memcpy(dst + dstByteOffset, src[ch] + srcByteOffset, chanByteLength);
            dstByteOffset += chanByteLength;
        }
    }
}

inline void setResamplerOutput(uint8_t** out, uint8_t* data, std::vector<BufferInfoHeader> *headers, size_t samplePosition = 0)
{
    size_t offset = samplePosition * componentSize(headers->at(0).componentType);

    if (headers->at(0).bufferViewByteStride == 0)
    {
        // mono-channel or planar buffer format
        for (size_t h = 0; h < headers->size(); h++)
        {
            out[h] = data + headers->at(h).byteOffset + offset;
        }
    } else {
        // interleaved/packed audio channels
        offset *= headers->size();
        out[0] = data + headers->at(0).byteOffset + offset;
    }
}

inline void setResamplerOutput(uint8_t** out, uint8_t* data, std::vector<BufferInfo> *bufferInfo, size_t samplePosition = 0)
{
    size_t offset = samplePosition * componentSize(bufferInfo->at(0).componentType);
    if (bufferInfo->at(0).stride == 0)
    {
        // mono-channel or planar buffer format
        for (size_t i = 0; i < bufferInfo->size(); i++)
        {
            out[i] = data + bufferInfo->at(i).offset + offset;
        }
    } else {
        // interleaved/packed audio channels
        offset *= bufferInfo->size();
        out[0] = data + bufferInfo->at(0).offset + offset;
    }
}


inline AVSampleFormat GetAVSampleFormat(ComponentType ct, bool planar)
{
    switch (ct)
    {
    case ComponentType::FLOAT:
        if (planar)
            return AVSampleFormat::AV_SAMPLE_FMT_FLTP;
        return AVSampleFormat::AV_SAMPLE_FMT_FLT;
    case ComponentType::SHORT:
        if (planar)
            return AVSampleFormat::AV_SAMPLE_FMT_S16P;
        return AVSampleFormat::AV_SAMPLE_FMT_S16;
    case ComponentType::UNSIGNED_BYTE:
        if (planar)
            return AVSampleFormat::AV_SAMPLE_FMT_U8P;
        return AVSampleFormat::AV_SAMPLE_FMT_U8;
    default:
        return AVSampleFormat::AV_SAMPLE_FMT_NONE;
    }
}

void AudioDecoder::DecodeInto(MafBuffer *mafBuffer)
{
    BufferHandler* handler = mafBuffer->first;

    if (handler->getFrames()[0].data() == nullptr){
        throw "buffer not allocated";
    }

    Frame* mafFrame = getWritableFrame(handler);

    std::vector<BufferInfo> bufferInfo = mafBuffer->second;
    std::vector<BufferInfoHeader> *headers = nullptr;

    AVFrame *avFrame = av_frame_alloc();

#if ENABLE_AUDIO_RESAMPLING
    struct SwrContext *swr = nullptr;
    std::vector<uint8_t*> swrOut;
#endif

    try
    {

    // check expected output format
    if (bufferInfo.size() == 0)
    {
        throw "missing BufferInfo";
    }
    int outputSampleSize = componentSize(bufferInfo.at(0).componentType);
    int outputNumChannels = (int)bufferInfo.size();
    bool planarAudioFormat = outputNumChannels > 1 ? bufferInfo.at(0).stride > 0 : true;
    AVSampleFormat outputSampleFmt = GetAVSampleFormat(bufferInfo.at(0).componentType, planarAudioFormat);
    const int inputSampleRate = codecCtx_->sample_rate;
    const int outputSampleRate = bufferInfo.at(0).targetSampleRate > 0 ? bufferInfo.at(0).targetSampleRate : inputSampleRate;
    // The timed accessor information header is written when the scene signals it (a bufferView on the timed accessor).
    const bool useAccessorHeaders = bufferInfo.at(0).timedHeader;
    // one buffer carries one signal: all its accessors share the sample rate and the playback speed
    for (const auto &bi : bufferInfo)
    {
        if (bi.targetSampleRate != bufferInfo.at(0).targetSampleRate || bi.playbackSpeed != bufferInfo.at(0).playbackSpeed)
        {
            throw "audio accessors of one buffer have different targetSampleRate or playbackSpeed";
        }
    }
    // playbackSpeed (MPEG_audio_spatial, 0.5 to 2.0): the resampler reads the samples at speed times the decoded
    // rate and writes them at the output rate, so the signal plays faster or slower and its pitch follows.
    const float playbackSpeed = bufferInfo.at(0).playbackSpeed;
    if (!(playbackSpeed >= 0.5f && playbackSpeed <= 2.0f))
    {
        throw "playbackSpeed is outside 0.5 to 2.0";
    }
    const int resamplerInputRate = playbackSpeed == 1.0f ? inputSampleRate : (int)std::lround(inputSampleRate * (double)playbackSpeed);
    const double timeBase = av_q2d(stream_->time_base);

    /*
        Normaly, the maf frame audio buffer is filled entirely with samples, and sample count can be derived from buffer format and size.
        One decoded audio frame can result in 0 or more maf frames being written.
        However, when useAccessorHeaders == true, each decoded audio frame will be copied to a maf frame.
        In that case, the maf frame buffer may be too large,
        the frame accessors headers provide the sample count - if the maf frame buffer isn't large enough,
        the decoded audio frame is written to several maf frames.
    */

    int accessorsHeadersLength = 0;
    if (useAccessorHeaders)
    {
        headers = BufferInfoHeader::CreateAccessorsHeaders(&(bufferInfo), bufferInfo.at(0).immutable);
        BufferInfoHeader::ComputeTotalHeadersLength(headers, accessorsHeadersLength);
    }

    bool needsResampling = outputNumChannels != codecCtx_->ch_layout.nb_channels;
    if (codecCtx_->sample_fmt != outputSampleFmt){
        needsResampling = true;
    }
    if (av_sample_fmt_is_planar(codecCtx_->sample_fmt) && !planarAudioFormat){
        needsResampling = true;
    }
    if (outputSampleRate != resamplerInputRate){
        needsResampling = true;
    }
    if (needsResampling){
#if ENABLE_AUDIO_RESAMPLING
        if (outputSampleFmt == AVSampleFormat::AV_SAMPLE_FMT_NONE)
        {
            throw "unsupported audio sample format (componentType/stride)";
        }
        AVChannelLayout inLayout = {};
        AVChannelLayout outLayout = {};
        if (codecCtx_->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        {
            av_channel_layout_default(&inLayout, codecCtx_->ch_layout.nb_channels);
        }
        else
        {
            av_channel_layout_copy(&inLayout, &codecCtx_->ch_layout);
        }
        av_channel_layout_default(&outLayout, outputNumChannels);
        int swrErr = swr_alloc_set_opts2(&swr, &outLayout, outputSampleFmt, outputSampleRate,
                                         &inLayout, codecCtx_->sample_fmt, resamplerInputRate, 0, nullptr);
        av_channel_layout_uninit(&inLayout);
        av_channel_layout_uninit(&outLayout);
        if (swrErr < 0 || swr == nullptr || swr_init(swr) < 0)
        {
            throw "Resampler has not been properly initialized";
        }
        swrOut.assign(bufferInfo.size(), nullptr);
#else
        throw "buffer format doesn't match libav's decoder output format";
#endif
    }

        if (maxFrameBytes_ <= (uint32_t)accessorsHeadersLength)
        {
            throw "buffer frame is too small for audio samples";
        }
        int dstSampleCapacity = (maxFrameBytes_ - accessorsHeadersLength) / ((int)bufferInfo.size()*componentSize(bufferInfo.at(0).componentType));
        int dstSamplesWritten = 0;
        int srcSamplesRead = -1;
        double avFrameSourceTime = 0.0;   // source time of the first sample of avFrame, in seconds
        double nextSourceTime = rangeStart_;
        double lastEndSource = rangeStart_;   // source time at the end of the last decoded frame that was output
        double base = 0.0;
        DecoderQueueItem pending;
        bool draining = false;

#if ENABLE_AUDIO_RESAMPLING
        // At the end of a pass (loop marker or end of the media) the resampler still holds the last samples of the pass:
        // output them, then reset the resampler so that the next pass starts without the filter state of this one.
        auto flushResampler = [&]()
        {
            // end of the signal on the buffer timeline, in source seconds; a pass that loops ends where the next one starts,
            // so that buffer timestamps keep increasing
            double passEnd = base + (lastEndSource - rangeStart_);
            if (pending.kind == DecoderQueueItem::LOOP)
            {
                passEnd = std::min(passEnd, pending.base);
            }
            while (!quit_)
            {
                if (swr_get_delay(swr, outputSampleRate) <= 0)
                {
                    break;
                }
                while (mafFrame == nullptr && !quit_)
                {
                    mafFrame = getWritableFrame(handler);
                    if (mafFrame == nullptr)
                    {
                        idleSleep();
                    }
                }
                if (mafFrame == nullptr)
                {
                    break;
                }
                uint8_t* pData = mafFrame->data;
                int written;
                if (useAccessorHeaders)
                {
                    for (auto &h : *headers)
                    {
                        h.count = dstSampleCapacity;
                    }
                    if (!planarAudioFormat)
                    {
                        BufferInfoHeader::ComputeAccessorsHeadersInterleaved(headers);
                    }
                    else
                    {
                        BufferInfoHeader::ComputeAccessorsHeadersPlanar(headers);
                    }
                    pData += accessorsHeadersLength;
                    setResamplerOutput(swrOut.data(), pData, headers, 0);
                    written = swr_convert(swr, swrOut.data(), dstSampleCapacity, nullptr, 0);
                    if (written <= 0)
                    {
                        break;
                    }
                    for (auto &h : *headers)
                    {
                        h.count = written;
                    }
                    mafFrame->timestamp = getTimestamp(std::max(0.0, passEnd / playbackSpeed - written / (double)outputSampleRate));
                    BufferInfoHeader::WriteAccessorsHeaders(mafFrame->data, headers);
                    dstSamplesWritten = dstSampleCapacity;
                }
                else
                {
                    setResamplerOutput(swrOut.data(), pData, &bufferInfo, dstSamplesWritten);
                    written = swr_convert(swr, swrOut.data(), dstSampleCapacity - dstSamplesWritten, nullptr, 0);
                    if (written <= 0)
                    {
                        break;
                    }
                    if (dstSamplesWritten == 0)
                    {
                        mafFrame->timestamp = getTimestamp(std::max(0.0, passEnd / playbackSpeed - written / (double)outputSampleRate));
                    }
                    dstSamplesWritten += written;
                }
                if (dstSamplesWritten == dstSampleCapacity)
                {
                    while (!quit_)
                    {
                        try {
                            handler->writeFrame2(mafFrame);
                            break;
                        } catch (...){
                            idleSleep();
                            continue;
                        }
                    }
                    mafFrame = nullptr;
                    dstSamplesWritten = 0;
                }
            }
            swr_close(swr);
            if (swr_init(swr) < 0)
            {
                throw "Resampler has not been properly initialized";
            }
        };
#endif

        while (!quit_)
        {
            bool progress = false;

            // deque packets and feed decoder
            if (!draining)
            {
                DecoderQueueItem item;
                if (queue_.Front(item))
                {
                    if (item.kind == DecoderQueueItem::PACKET)
                    {
                        int err = avcodec_send_packet(codecCtx_, item.pkt);
                        if (err >= 0)
                        {
                            queue_.Pop();
                            av_packet_free(&item.pkt);
                            progress = true;
                        }
                        else if (err != AVERROR(EAGAIN))
                        {
                            queue_.Pop();
                            av_packet_free(&item.pkt);
                            throw err;
                        }
                    }
                    else
                    {
                        // output the frames still held by the decoder
                        int err = avcodec_send_packet(codecCtx_, nullptr);
                        if (err >= 0 || err == AVERROR_EOF)
                        {
                            queue_.Pop();
                            pending = item;
                            draining = true;
                            progress = true;
                        }
                        else if (err != AVERROR(EAGAIN))
                        {
                            throw err;
                        }
                    }
                }
            }

            // acquire writable maf output buffer
            if (mafFrame == nullptr)
                mafFrame = getWritableFrame(handler);

            // try decoding an audio frame
            if (srcSamplesRead < 0){
                int err1 = avcodec_receive_frame(codecCtx_, avFrame);
                if (err1 == 0)
                {
                    progress = true;
                    int64_t pts = avFrame->pts;
                    avFrameSourceTime = (pts != AV_NOPTS_VALUE) ? pts * timeBase : nextSourceTime;
                    nextSourceTime = avFrameSourceTime + avFrame->nb_samples / (double)inputSampleRate;
                    bool dropped = (trimStart_ && nextSourceTime <= rangeStart_ + 1e-6) || (rangeEnd_ >= 0.0 && avFrameSourceTime >= rangeEnd_);
                    if (dropped)
                    {
                        av_frame_unref(avFrame);
                    }
                    else
                    {
                        srcSamplesRead = 0;
                        // 4.3: the media is consumed from startTimeOffset to endTimeOffset: a decoded frame that straddles
                        // either limit is cut at the sample, not at the codec frame
                        if (trimStart_ && avFrameSourceTime < rangeStart_)
                        {
                            int skip = (int)std::min<double>(avFrame->nb_samples, std::ceil((rangeStart_ - avFrameSourceTime) * inputSampleRate - 1e-6));
                            srcSamplesRead = std::max(0, skip);
                        }
                        if (rangeEnd_ >= 0.0 && nextSourceTime > rangeEnd_)
                        {
                            int keep = (int)std::max<double>(0.0, std::floor((rangeEnd_ - avFrameSourceTime) * inputSampleRate + 1e-6));
                            avFrame->nb_samples = std::min(avFrame->nb_samples, keep);
                            nextSourceTime = avFrameSourceTime + avFrame->nb_samples / (double)inputSampleRate;
                        }
                        lastEndSource = nextSourceTime;
                        if (srcSamplesRead >= avFrame->nb_samples)
                        {
                            av_frame_unref(avFrame);
                            srcSamplesRead = -1;
                        }
                    }
                }
                else if (err1 == AVERROR_EOF)
                {
                    // everything before the marker has been output
#if ENABLE_AUDIO_RESAMPLING
                    if (swr)
                    {
                        flushResampler();
                    }
#endif
                    avcodec_flush_buffers(codecCtx_);
                    draining = false;
                    progress = true;
                    if (pending.kind == DecoderQueueItem::LOOP)
                    {
                        base = pending.base;
                        nextSourceTime = rangeStart_;
                    }
                    else
                    {
                        break;
                    }
                }
                else if (err1 != AVERROR(EAGAIN))
                {
                    throw err1;
                }
            }

            // resample / copy frame to output buffer
            if (mafFrame != nullptr && srcSamplesRead >= 0)
            {
                    progress = true;
                    uint8_t* pData = mafFrame->data;
                    if (dstSamplesWritten == 0 && !useAccessorHeaders){
                        mafFrame->timestamp = getTimestamp(std::max(0.0, (base + (avFrameSourceTime - rangeStart_) + srcSamplesRead / (double)inputSampleRate) / playbackSpeed));
                    }

                    if (useAccessorHeaders)
                    {
                        // one maf frame holds as many decoded samples as its capacity allows
                        int remaining = avFrame->nb_samples - srcSamplesRead;
                        int chunk = std::min(remaining, dstSampleCapacity);
                        if (dstSampleCapacity <= 0)
                        {
                            throw "insufficient maf buffer frame capacity";
                        }
#if ENABLE_AUDIO_RESAMPLING
                        if (swr){
                            chunk = remaining;
                            while (chunk > 1 && swr_get_out_samples(swr, chunk) > dstSampleCapacity){
                                chunk /= 2;
                            }
                            if (swr_get_out_samples(swr, chunk) > dstSampleCapacity){
                                throw "insufficient maf buffer frame capacity";
                            }
                        }
#endif
                        // with a resampler the output count is only known after the conversion: channel planes are laid out for the capacity
                        int layoutCount = swr ? dstSampleCapacity : chunk;
                        for (auto &h : *headers)
                        {
                            h.count = layoutCount;
                        }
                        if (!planarAudioFormat)
                        {
                            BufferInfoHeader::ComputeAccessorsHeadersInterleaved(headers);
                        }
                        else
                        {
                            BufferInfoHeader::ComputeAccessorsHeadersPlanar(headers);
                        }
                        pData += accessorsHeadersLength;

                        int writtenCount = chunk;
                        if (needsResampling){
#if ENABLE_AUDIO_RESAMPLING
                            setResamplerOutput(swrOut.data(), pData, headers, 0);
                            const uint8_t* swrIn[AV_NUM_DATA_POINTERS] = {};
                            int inBytes = av_get_bytes_per_sample(codecCtx_->sample_fmt);
                            int inChannels = codecCtx_->ch_layout.nb_channels;
                            if (av_sample_fmt_is_planar(codecCtx_->sample_fmt)){
                                for (int ch = 0; ch < inChannels && ch < AV_NUM_DATA_POINTERS; ch++){
                                    swrIn[ch] = avFrame->extended_data[ch] + (size_t)srcSamplesRead * inBytes;
                                }
                            } else {
                                swrIn[0] = avFrame->extended_data[0] + (size_t)srcSamplesRead * inBytes * inChannels;
                            }
                            writtenCount = swr_convert(swr, swrOut.data(), dstSampleCapacity, swrIn, chunk);
                            if (writtenCount < 0){
                                throw "resampling error";
                            }
#else
                            throw "resampling disabled";
#endif
                        } else {
                            copyAudioSamples(avFrame->data, srcSamplesRead, pData, 0, chunk, outputSampleSize, outputNumChannels, !planarAudioFormat);
                        }
                        for (auto &h : *headers)
                        {
                            h.count = writtenCount;
                        }
                        mafFrame->timestamp = getTimestamp(std::max(0.0, (base + (avFrameSourceTime - rangeStart_) + srcSamplesRead / (double)inputSampleRate) / playbackSpeed));
                        BufferInfoHeader::WriteAccessorsHeaders(mafFrame->data, headers);
                        srcSamplesRead += chunk;
                        if (srcSamplesRead >= avFrame->nb_samples){
                            av_frame_unref(avFrame);
                            srcSamplesRead = -1;
                        }
                        dstSamplesWritten = dstSampleCapacity;
                    }
                    else if (needsResampling){
#if ENABLE_AUDIO_RESAMPLING
                        setResamplerOutput(swrOut.data(), pData, &bufferInfo, dstSamplesWritten);
                        const uint8_t* swrIn[AV_NUM_DATA_POINTERS] = {};
                        int inChannels = codecCtx_->ch_layout.nb_channels;
                        if (av_sample_fmt_is_planar(codecCtx_->sample_fmt)){
                            for (int ch = 0; ch < inChannels && ch < AV_NUM_DATA_POINTERS; ch++){
                                swrIn[ch] = avFrame->extended_data[ch];
                            }
                        } else {
                            swrIn[0] = avFrame->extended_data[0];
                        }
                        int numSamplesOut = swr_convert(swr, swrOut.data(), dstSampleCapacity-dstSamplesWritten, swrIn, avFrame->nb_samples);
                        if(numSamplesOut < 0){
                            throw "resampling error";
                        }
                        dstSamplesWritten += numSamplesOut;
                        srcSamplesRead = -1;
                        av_frame_unref(avFrame);
#else
                        throw "resampling disabled";
#endif
                    } else {
                        int numSamples = std::min(avFrame->nb_samples-srcSamplesRead, dstSampleCapacity-dstSamplesWritten);
                        copyAudioSamples(avFrame->data, srcSamplesRead, pData, dstSamplesWritten, numSamples, outputSampleSize, outputNumChannels, !planarAudioFormat);
                        srcSamplesRead += numSamples;
                        dstSamplesWritten += numSamples;
                        if(srcSamplesRead == avFrame->nb_samples){
                            av_frame_unref(avFrame);
                            srcSamplesRead = -1;
                        }
                    }
                    if(dstSamplesWritten == dstSampleCapacity){
                        while (!quit_){
                            try {
                                handler->writeFrame2(mafFrame);
                                break;
                            } catch (...){
                                idleSleep();
                                continue;
                            }
                        }
                        mafFrame = nullptr;
                        dstSamplesWritten = 0;
                    }
            }

            if (!progress)
            {
                idleSleep();
            }
        }
    }
    catch (const int &ex)
    {
        char errText[128]; av_strerror(ex, errText, sizeof errText);
        decLog("decoder error: %d (%s)", ex, errText);
        error_ = make_exception_ptr(std::runtime_error(std::to_string(ex)));
        failed_ = true;
    }
    catch (const char *ex)
    {
        decLog("decoder error: %s", ex);
        error_ = make_exception_ptr(std::runtime_error(std::string(ex)));
        failed_ = true;
    }
    catch (...)
    {
        error_ = std::current_exception();
        failed_ = true;
    }

#if ENABLE_AUDIO_RESAMPLING
    swr_free(&swr);
#endif
    av_frame_free(&avFrame);
    if (headers)
        delete headers;
    done_ = true;
}

void AudioDecoder::Start(MafBuffer *mafBuffer)
{
    if (IsRunning())
    {
        return;
    }
    quit_ = false;
    // every frame of a buffer is allocated with the same size
    if (maxFrameBytes_ == 0 && !mafBuffer->first->getFrames()->empty())
    {
        maxFrameBytes_ = mafBuffer->first->getFrames()->at(0).length;
    }
    thread_ = std::thread(&AudioDecoder::DecodeInto, this, mafBuffer);
}

void AudioDecoder::Stop()
{
    quit_ = true;
    if (IsRunning())
    {
        thread_.join();
    }
}

bool AudioDecoder::IsRunning()
{
    return thread_.joinable();
}
