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

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <MetadataDecoder.hpp>

#ifdef __ANDROID__
#include <cstdio>
#include <cstdarg>
static void metaLog(const char *fmt, ...)
{
    FILE *f = fopen("/sdcard/Android/data/com.fivegmag.rtxrplayer/files/avpipeline.log", "a");
    if (!f)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}
#else
#include <cstdio>
#include <cstdarg>
static void metaLog(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
#endif

namespace
{
    // returns the frame at writeIdx unless it is the current presentation frame
    Frame *writableFrame(BufferHandler *handler)
    {
        if (handler->capacity() < 2)
            return nullptr;
        if ((handler->getWriteIdx() == handler->getReadIdx()) && (handler->count() > 0))
            return nullptr;
        return &handler->getFrames()->at(handler->getWriteIdx());
    }

    void sleepShort()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
} // namespace

bool MetadataDecoder::KindFromTag(uint32_t codecTag, Kind &kind)
{
    switch (codecTag)
    {
    case MKTAG('m', 'e', 't', 't'):
        kind = Kind::SCENE_UPDATE;
        return true;
    case MKTAG('g', 'l', 'a', 't'):
        kind = Kind::ANIMATION;
        return true;
    case MKTAG('v', 'c', 'o', 'r'):
        kind = Kind::CORRESPONDENCE;
        return true;
    case MKTAG('p', 'o', 's', 't'):
        kind = Kind::POSE;
        return true;
    default:
        return false;
    }
}

Decoder *CreateMetadataDecoder(AVFormatContext *fmtCtx, uint32_t streamIdx)
{
    if (streamIdx >= fmtCtx->nb_streams)
    {
        throw "invalid stream id";
    }
    AVStream *stream = fmtCtx->streams[streamIdx];
    MetadataDecoder::Kind kind;
    if (!MetadataDecoder::KindFromTag(stream->codecpar->codec_tag, kind))
    {
        metaLog("stream %u: no carriage format is defined for the sample entry type 0x%08x", streamIdx, (unsigned)stream->codecpar->codec_tag);
        throw "unsupported metadata sample entry type";
    }
    return new MetadataDecoder(stream, kind);
}

MetadataDecoder::MetadataDecoder(AVStream *stream, Kind kind) : stream_(stream), kind_(kind)
{
}

MetadataDecoder::~MetadataDecoder()
{
    Stop();
}

void MetadataDecoder::QueuePacket(AVPacket *pkt)
{
    queue_.PushPacket(pkt);
}

void MetadataDecoder::SetRange(double startSource_s, double endSource_s, bool trimStart)
{
    rangeStart_ = startSource_s;
    rangeEnd_ = endSource_s;
    trimStart_ = trimStart;
}

void MetadataDecoder::QueueLoop(double newBase_s)
{
    queue_.PushMarker(DecoderQueueItem::LOOP, newBase_s);
}

void MetadataDecoder::QueueEnd()
{
    queue_.PushMarker(DecoderQueueItem::END, 0.0);
}

bool MetadataDecoder::IsDone()
{
    return done_;
}

bool MetadataDecoder::HasError()
{
    return failed_;
}

size_t MetadataDecoder::QueuedPackets()
{
    return queue_.Size();
}

void MetadataDecoder::DecodeInto(MafBuffer *mafBuffer)
{
    BufferHandler *handler = mafBuffer->first;
    const std::vector<BufferInfo> infos = mafBuffer->second;

    try
    {
        if (infos.empty())
        {
            throw "missing BufferInfo";
        }
        if (handler->getFrames()->empty() || handler->getFrames()->at(0).data == nullptr)
        {
            throw "buffer not allocated";
        }
        const BufferInfo &info = infos.at(0);
        const bool sceneUpdateBuffer = info.attributeType == AttributeType::METADATA_SCENE_UPDATE;
        const bool halted = false;

        if (kind_ == Kind::SCENE_UPDATE && !sceneUpdateBuffer)
        {
            throw "a 'mett' track carries scene updates: its buffer has no accessor (METADATA_SCENE_UPDATE)";
        }
        if (kind_ == Kind::ANIMATION)
        {
            if (sceneUpdateBuffer)
            {
                throw "a 'glat' track is read through a timed accessor, not a scene update buffer";
            }
            // 5.5.2.2: "The componentType of the referenced accessor shall be BYTE and the type shall be SCALAR"
            if (info.componentType != ComponentType::BYTE || info.type != SampleType::SCALAR)
            {
                throw "a 'glat' track requires an accessor with componentType BYTE and type SCALAR";
            }
        }

        // layout of a frame of an accessor buffer: the timed accessor information headers of the accessors of the buffer
        // (Table 8), then the sample bytes, which belong to the first accessor; the other accessors of the buffer get count 0
        std::vector<BufferInfoHeader> headers;
        int headersLength = 0;
        if (!sceneUpdateBuffer && info.timedHeader)
        {
            headers.resize(infos.size());
            for (size_t i = 0; i < infos.size(); i++)
            {
                headers[i].immutable = infos[i].immutable;
                headers[i].componentType = infos[i].componentType;
                headers[i].type = infos[i].type;
                headers[i].normalized = infos[i].normalized;
                headersLength += headers[i].GetHeaderSize();
            }
        }
        const uint32_t dataOffset = sceneUpdateBuffer ? (uint32_t)SCENE_UPDATE_PREFIX_BYTES
                                    : (info.timedHeader ? (uint32_t)headersLength : (info.offset > 0 ? (uint32_t)info.offset : 0));
        const size_t itemSize = (size_t)componentSize(info.componentType) * (size_t)sampleSize(info.type);
        if (!halted && maxFrameBytes_ < dataOffset)
        {
            metaLog("buffer frame of %u bytes, accessor headers need %u bytes", maxFrameBytes_, dataOffset);
            throw "buffer frame is too small for the accessor headers";
        }

        double base = 0.0;
        bool loggedHalt = false;
        long long dropped = 0;
        const double timeBase = av_q2d(stream_->time_base);

        while (!quit_)
        {
            DecoderQueueItem item;
            if (!queue_.Front(item))
            {
                sleepShort();
                continue;
            }
            if (item.kind == DecoderQueueItem::LOOP)
            {
                queue_.Pop();
                base = item.base;
                continue;
            }
            if (item.kind == DecoderQueueItem::END)
            {
                queue_.Pop();
                break;
            }

            AVPacket *pkt = item.pkt;
            const int64_t ts = pkt->pts != AV_NOPTS_VALUE ? pkt->pts : pkt->dts;
            const double sourceTime = ts != AV_NOPTS_VALUE ? ts * timeBase : rangeStart_;
            const bool outsideRange = (trimStart_ && sourceTime < rangeStart_ - 1e-6) || (rangeEnd_ >= 0.0 && sourceTime >= rangeEnd_);

            if (halted || outsideRange)
            {
                if (halted && !loggedHalt)
                {
                    loggedHalt = true;
                    metaLog("unverified: could not obtain ISO/IEC 14496-12, the '%s' samples are not published",
                            kind_ == Kind::CORRESPONDENCE ? "vcor" : "post");
                }
                queue_.Pop();
                av_packet_free(&pkt);
                continue;
            }

            Frame *frame = writableFrame(handler);
            if (frame == nullptr)
            {
                sleepShort();
                continue;
            }

            // 7.4.3.2 and 7.5.3.2: the sample syntax of 'vcor' and 'post' samples. ISOBMFF stores integers and floats big-endian:
            // the accessor of the scene reads host-endian elements of its own component size.
            std::vector<uint8_t> converted;
            const uint8_t *srcData = pkt->data;
            size_t srcSize = (size_t)pkt->size;
            if (kind_ == Kind::CORRESPONDENCE && srcSize >= 4)
            {
                const uint32_t vertexCount = ((uint32_t)pkt->data[0] << 24) | ((uint32_t)pkt->data[1] << 16) | ((uint32_t)pkt->data[2] << 8) | pkt->data[3];
                const size_t body = srcSize - 4;
                const size_t precision = vertexCount > 0 ? body / vertexCount : 0;   // length in bytes of each face_index, 1 to 4 (7.4.2.3)
                const size_t outSize = (size_t)componentSize(info.componentType);
                if (vertexCount == 0 || precision < 1 || precision > 4 || precision * vertexCount != body || outSize < 1 || outSize > 4)
                {
                    metaLog("'vcor' sample at %.3f s does not follow the syntax of 7.4.3.2 (%zu bytes, vertex_count %u): dropped", sourceTime, srcSize, vertexCount);
                    queue_.Pop();
                    av_packet_free(&pkt);
                    continue;
                }
                converted.assign((size_t)vertexCount * outSize, 0);
                for (uint32_t v = 0; v < vertexCount; v++)
                {
                    uint32_t value = 0;
                    for (size_t b = 0; b < precision; b++) value = (value << 8) | pkt->data[4 + v * precision + b];
                    for (size_t b = 0; b < outSize; b++) converted[v * outSize + b] = (uint8_t)(value >> (8 * b));
                }
                srcData = converted.data();
                srcSize = converted.size();
            }
            else if (kind_ == Kind::POSE)
            {
                converted.assign(pkt->data, pkt->data + (srcSize - (srcSize % 4)));
                for (size_t b = 0; b + 3 < converted.size(); b += 4)
                {
                    std::swap(converted[b], converted[b + 3]);
                    std::swap(converted[b + 1], converted[b + 2]);
                }
                srcData = converted.data();
                srcSize = converted.size();
            }

            const size_t payload = srcSize;
            const size_t total = (size_t)dataOffset + payload;
            bool publish = true;
            if (total > maxFrameBytes_)
            {
                metaLog("metadata sample at %.3f s of %zu bytes does not fit a buffer frame of %u bytes (offset %u): dropped", sourceTime, payload, maxFrameBytes_, dataOffset);
                publish = false;
            }
            else if (!sceneUpdateBuffer && itemSize > 0 && (payload % itemSize) != 0)
            {
                metaLog("metadata sample at %.3f s of %zu bytes is not a whole number of accessor elements of %zu bytes: dropped", sourceTime, payload, itemSize);
                publish = false;
            }

            if (publish)
            {
                uint8_t *data = frame->data;
                if (sceneUpdateBuffer)
                {
                    // 7.2.1: sync samples hold glTF JSON documents, the other samples JSON patch documents
                    // libav marks every sample of a metadata track as a key sample, so the sync sample flag of the file is not
                    // available through it: a glTF document is a JSON object and a JSON patch document (RFC 6902) a JSON array.
                    bool document = false;
                    for (int k = 0; k < pkt->size; k++)
                    {
                        const uint8_t c = pkt->data[k];
                        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || (k == 0 && c >= 0xEF)) continue;
                        document = c == '{';
                        break;
                    }
                    data[0] = document ? SCENE_UPDATE_FLAG_SYNC : 0;
                    data[1] = SCENE_UPDATE_MODE_UNKNOWN;
                }
                else if (info.timedHeader)
                {
                    for (size_t i = 0; i < headers.size(); i++)
                    {
                        headers[i].timestampDelta = 0.0f;
                        headers[i].count = (i == 0 && itemSize > 0) ? (uint32_t)(payload / itemSize) : 0;
                    }
                    BufferInfoHeader::ComputeAccessorsHeadersPlanar(&headers);
                    BufferInfoHeader::WriteAccessorsHeaders(data, &headers);
                }
                if (payload > 0)
                {
                    memcpy(data + dataOffset, srcData, payload);
                }
                frame->timestamp = getTimestamp(base + std::max(0.0, sourceTime - rangeStart_));
                frame->length = (uint32_t)total;
                frame->extraFrameInfoFlags = 0;
                frame->width = 0;
                frame->height = 0;
                while (!quit_)
                {
                    try
                    {
                        handler->writeFrame2(frame);
                        break;
                    }
                    catch (...)
                    {
                        sleepShort();
                    }
                }
            }
            else
            {
                dropped++;
            }
            queue_.Pop();
            av_packet_free(&pkt);
        }
        if (dropped > 0)
        {
            metaLog("%lld metadata sample(s) dropped", dropped);
        }
    }
    catch (const int &ex)
    {
        error_ = std::make_exception_ptr(std::runtime_error(std::to_string(ex)));
        failed_ = true;
    }
    catch (const char *ex)
    {
        metaLog("metadata decoder: %s", ex);
        error_ = std::make_exception_ptr(std::runtime_error(std::string(ex)));
        failed_ = true;
    }
    catch (...)
    {
        error_ = std::current_exception();
        failed_ = true;
    }
    done_ = true;
}

void MetadataDecoder::Start(MafBuffer *mafBuffer)
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
    thread_ = std::thread(&MetadataDecoder::DecodeInto, this, mafBuffer);
}

void MetadataDecoder::Stop()
{
    quit_ = true;
    if (IsRunning())
    {
        thread_.join();
    }
}

bool MetadataDecoder::IsRunning()
{
    return thread_.joinable();
}
