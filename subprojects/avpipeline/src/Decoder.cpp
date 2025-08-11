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

// Attempt to reformat the decoded buffer to match the expected buffer layout
// Sample format and channel layout (planar/interleaved) is adressed, sample rate remains unchanged.
#define ENABLE_AUDIO_RESAMPLING 0

using ms = std::chrono::duration<double, std::milli>;


static Frame* getWritableFrame(BufferHandler* handler){
    // returns the frame at writeIdx unless it is the current presentation frame
    if (handler->capacity() < 2)
        return nullptr;
    if ((handler->getWriteIdx() == handler->getReadIdx()) && (handler->count() > 0))
        return nullptr;
    return &handler->getFrames()->at(handler->getWriteIdx());
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

VideoDecoder::VideoDecoder(AVStream *stream, AVCodecContext *ctx)
{
    stream_ = stream;
    codecCtx_ = ctx;
    pktQueueMx_ = new std::mutex();
    bufferUpdateMx_ = new std::mutex();
}

VideoDecoder::~VideoDecoder()
{
    Stop();
    if (codecCtx_)
    {
        avcodec_free_context(&codecCtx_);
    }

    delete pktQueueMx_;
    delete bufferUpdateMx_;
}

void VideoDecoder::QueuePacket(AVPacket *pkt)
{
    AVPacket *p = av_packet_alloc();
    if (!p)
    {
        av_packet_unref(pkt);
        throw "failed to queue packet";
    }
    av_packet_move_ref(p, pkt);
    std::lock_guard<std::mutex> lock(*pktQueueMx_);
    pktQueue_.push(p);
    return;
}

void VideoDecoder::DecodeInto(MafBuffer *mafBuffer)
{
    Frame* mafFrame = nullptr;
    BufferHandler* handler = mafBuffer->first;

    // decoding
    AVPacket *pkt;
    AVFrame *avFrame = av_frame_alloc();

    // conversion
    AVPixelFormat pixFmt = AV_PIX_FMT_RGB24; // FIXME: get format from BufferInfo
    AVFrame *dst = nullptr;
    struct SwsContext *swsCtx = nullptr;

    try
    {

        bool eof = false;

        while (!quit_)
        {

            int err = 0;
            int err1 = 0;

            if (pktQueue_.empty())
            {
                continue;
            }
            else
            {
                if (!eof)
                {
                    std::unique_lock<std::mutex> lock(*pktQueueMx_);
                    pkt = pktQueue_.front();
                    err = avcodec_send_packet(codecCtx_, pkt);
                    if (err >= 0)
                    {
                        av_packet_free(&pkt);
                        pktQueue_.pop();
                    }
                    else if (err != AVERROR(EAGAIN))
                    {
                        av_packet_free(&pkt);
                        throw printf("avcodec_send_packet AVERROR(%d)", err);
                    }
                    else if (err == AVERROR_EOF)
                    {
                        std::cout << printf("avcodec_send_packet AVERROR_EOF(%d)\n", err);
                        eof = true;
                    }
                    lock.unlock();
                }

                mafFrame = getWritableFrame(handler);
                if (mafFrame != nullptr)
                {
                    err1 = avcodec_receive_frame(codecCtx_, avFrame);
                     if (err1 == 0)
                    {
                        if (!dst)
                        {
                            dst = av_frame_alloc();
                            dst->width = avFrame->width;
                            dst->height = avFrame->height;
                            dst->format = pixFmt;
                            av_image_alloc(dst->data, dst->linesize, avFrame->width, avFrame->height, pixFmt, 1);
                        }
                        if (!swsCtx)
                        {
                            swsCtx = sws_getContext(
                                avFrame->width,
                                avFrame->height,
                                (enum AVPixelFormat)avFrame->format,
                                dst->width,
                                dst->height,
                                pixFmt,
                                SWS_FAST_BILINEAR | SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND,
                                NULL,
                                NULL,
                                NULL);

                            const int* srcCoeffs = nullptr;

                            switch (avFrame->colorspace)
                            {
                                case AVCOL_SPC_BT709:
                                    srcCoeffs = sws_getCoefficients(SWS_CS_ITU709);
                                    break;
                                case AVCOL_SPC_BT470BG: // same as BT.601 PAL
                                case AVCOL_SPC_SMPTE170M: // same as BT.601 NTSC
                                    srcCoeffs = sws_getCoefficients(SWS_CS_ITU601);
                                    break;
                                case AVCOL_SPC_BT2020_NCL:
                                case AVCOL_SPC_BT2020_CL:
                                    srcCoeffs = sws_getCoefficients(SWS_CS_BT2020);
                                    break;
                                default:
                                    // Fallback: guess BT.709 for HD, BT.601 for SD
                                    srcCoeffs = (avFrame->width >= 1280 || avFrame->height > 576)
                                        ? sws_getCoefficients(SWS_CS_ITU709)
                                        : sws_getCoefficients(SWS_CS_ITU601);
                                    break;
                            }

                            // -------------------------------------------------------
                            // Range: detect from frame metadata
                            // -------------------------------------------------------
                            int srcRange = (avFrame->color_range == AVCOL_RANGE_JPEG) ? 1 : 0; // 0=limited, 1=full
                            int dstRange = 1; // Always output full range to Unity

                            // Neutral adjustment values
                            int brightness = 0;
                            int contrast   = 1 << 16; // 1.0 in 16.16 fixed-point
                            int saturation = 1 << 16; // 1.0 in 16.16 fixed-point

                            // -------------------------------------------------------
                            // Apply colorspace + range settings
                            // -------------------------------------------------------
                            sws_setColorspaceDetails(
                                swsCtx,
                                srcCoeffs, srcRange,
                                srcCoeffs, dstRange,
                                brightness, contrast, saturation);
                        }
                        
                        sws_scale(swsCtx, avFrame->data, avFrame->linesize, 0, dst->height, dst->data, dst->linesize);
                        mafFrame->timestamp = getTimestamp(avFrame->pts * av_q2d(stream_->time_base));
                        err1 = av_image_copy_to_buffer(mafFrame->data, mafFrame->length,
                                                       (const uint8_t *const *)dst->data, dst->linesize,
                                                       pixFmt, dst->width, dst->height, 1);
                        av_frame_unref(avFrame);
                        if (err1 < 0)
                        {
                            throw "failed to copy image buffer";
                        }
                        while (!quit_){
                            try {
                                handler->writeFrame2(mafFrame);
                                break;
                            } catch (...){
                                continue;
                            }
                        }
                    }
                    else if (err1 == AVERROR(EAGAIN))
                    {
                        if (err == AVERROR(EAGAIN))
                        {
                            throw "invalid decoder state";
                        }
                    }
                    else if (err)
                    {
                        throw printf("avcodec_receive_frame AVERROR(%d)", err);
                    }
                }
            }
        }
    }
    catch (const int &ex)
    {
        error_ = make_exception_ptr(std::runtime_error(std::to_string(ex)));
    }
    catch (const char *ex)
    {
        error_ = make_exception_ptr(std::runtime_error(std::string(ex)));
    }
    catch (...)
    {
        error_ = std::current_exception();
    }

    av_frame_free(&avFrame);
    if (dst)
    {
        av_freep(&dst->data[0]); // free av_image_alloc from yuv/rgb conversion
        av_frame_free(&dst);
    }
    if (swsCtx)
        sws_freeContext(swsCtx);
}

void VideoDecoder::Start(MafBuffer *mafBuffer)
{
    if (IsRunning())
    {
        return;
    }
    quit_ = false;
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
    pktQueueMx_ = new std::mutex();
    bufferUpdateMx_ = new std::mutex();
}

AudioDecoder::~AudioDecoder()
{
    Stop();
    if (codecCtx_)
    {
        avcodec_free_context(&codecCtx_);
    }

    delete pktQueueMx_;
    delete bufferUpdateMx_;
}

void AudioDecoder::QueuePacket(AVPacket *pkt)
{
    AVPacket *p = av_packet_alloc();
    if (!p)
    {
        av_packet_unref(pkt);
        throw "failed to queue packet";
    }
    // TODO: drop late packets
    av_packet_move_ref(p, pkt);
    std::lock_guard<std::mutex> lock(*pktQueueMx_);
    pktQueue_.push(p);
    return;
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
    
    int64_t pts = -1;
    int64_t pto = 0;
    uint64_t totalWrittenSamples = 0;
    

    if (handler->getFrames()[0].data() == nullptr){
        throw "buffer not allocated";
    }

    Frame* mafFrame = getWritableFrame(handler);
    
    std::vector<BufferInfo> bufferInfo = mafBuffer->second;
    std::vector<BufferInfoHeader> *headers = nullptr;

    AVPacket *pkt;
    AVFrame *avFrame = av_frame_alloc();

#if ENABLE_AUDIO_RESAMPLING
    struct SwrContext *swr = nullptr;
    uint8_t **swrOut = nullptr;
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

    /*
        Normaly, the maf frame audio buffer is filled entirely with samples, and sample count can be derived from buffer format and size. 
        One decoded audio frame can result in 0 or more maf frames being written.
        However, when useAccessorHeaders_ == true, each decoded audio frame will be copied to a maf frame.
        In that case, the maf frame buffer may be too large, 
        the frame accessors headers provide the sample count - if the maf frame buffer isn't large enough, an error is thrown.
    */
    
    int accessorsHeadersLength = 0;
    if (useAccessorHeaders_)
    {
        headers = BufferInfoHeader::CreateAccessorsHeaders(&(bufferInfo), true);
        BufferInfoHeader::ComputeTotalHeadersLength(headers, accessorsHeadersLength);
    }

    bool needsResampling = outputNumChannels != codecCtx_->ch_layout.nb_channels;
    if (codecCtx_->sample_fmt != outputSampleFmt){
        needsResampling = true;
    }
    if (av_sample_fmt_is_planar(codecCtx_->sample_fmt) && !planarAudioFormat){
        needsResampling = true;
    }
    if (needsResampling){
#if ENABLE_AUDIO_RESAMPLING
        swr = swr_alloc();
        av_opt_set_int(swr, "in_channel_count", codecCtx_->ch_layout.nb_channels, 0);
        av_opt_set_int(swr, "in_channel_layout", codecCtx_->ch_layout.u.mask, 0);
        av_opt_set_int(swr, "in_sample_rate", codecCtx_->sample_rate, 0);
        av_opt_set_sample_fmt(swr, "in_sample_fmt", codecCtx_->sample_fmt, 0);

        swrOut = new uint8_t*[bufferInfo.size()]();
        av_opt_set_int(swr, "out_channel_count", outputNumChannels, 0);
        av_opt_set_int(swr, "out_channel_layout", 0, 0);
        if (outputSampleFmt == AVSampleFormat::AV_SAMPLE_FMT_NONE)
        {
            throw "unsupported audio sample format (componentType/stride)";
        }
        av_opt_set_sample_fmt(swr, "out_sample_fmt", outputSampleFmt, 0);
        av_opt_set_int(swr, "out_sample_rate", codecCtx_->sample_rate, 0);
        swr_init(swr);
        if (!swr_is_initialized(swr))
        {
            throw "Resampler has not been properly initialized";
        }
#else
        throw "buffer format doesn't match libav's decoder output format";
#endif
    }


        bool eof = false;

        int dstSampleCapacity = (mafFrame->length-accessorsHeadersLength) / ((int)bufferInfo.size()*componentSize(bufferInfo.at(0).componentType));
        int dstSamplesWritten = 0;
        int srcSamplesRead = -1;

        while (!quit_)
        {
            int err = 0;
            int err1 = 0;
            if (pktQueue_.empty())
            {
                continue;
            }
            else
            {
                // deque packets and feed decoder
                if (!eof)
                {
                    std::unique_lock<std::mutex> lock(*pktQueueMx_);
                    pkt = pktQueue_.front();
                    err = avcodec_send_packet(codecCtx_, pkt);
                    if (err >= 0)
                    {
                        av_packet_free(&pkt);
                        pktQueue_.pop();
                    }
                    else if (err != AVERROR(EAGAIN))
                    {
                        av_packet_free(&pkt);
                        throw printf("avcodec_send_packet AVERROR(%d)", err);
                    }
                    else if (err == AVERROR_EOF)
                    {
                        std::cout << printf("avcodec_send_packet AVERROR_EOF(%d)\n", err);
                        eof = true;
                    }
                    lock.unlock();
                }

                // acquire writable maf output buffer
                if (mafFrame == nullptr)
                    mafFrame = getWritableFrame(handler);
                
                // try decoding an audio frame
                if (srcSamplesRead < 0){
                    err1 = avcodec_receive_frame(codecCtx_, avFrame);
                    if (err1 == 0)
                    {
                        srcSamplesRead = 0;
                        if (pts < 0){
                            pto = avFrame->pts;
                        }
                    } else if (err1 == AVERROR(EAGAIN))
                    {
                        if (err == AVERROR(EAGAIN))
                        {
                            throw "invalid decoder state";
                        }
                    }
                    else if (err)
                    {
                        throw printf("avcodec_receive_frame AVERROR(%d)", err);
                    }
                }

                // resample / copy frame to output buffer
                if (mafFrame != nullptr && srcSamplesRead >= 0)
                {
                        if (srcSamplesRead == 0){
                            mafFrame->timestamp = getTimestamp(totalWrittenSamples / (double)codecCtx_->sample_rate );
                        }
                        uint8_t* pData = mafFrame->data;
                        
                        if (useAccessorHeaders_)
                        {
                            int count = avFrame->nb_samples;
#if ENABLE_AUDIO_RESAMPLING
                            if (needsResampling){
                                count = swr_get_out_samples(swr, count);
                            }
#endif
                            if (count > dstSampleCapacity){
                                // when writing headers, this implementation writes one decoded frame to the output
                                throw "insufficient maf buffer frame capacity";
                            }
                            for (auto &h : *headers)
                            {
                                h.count = count;
                            }
                            if (!planarAudioFormat)
                            {
                                BufferInfoHeader::ComputeAccessorsHeadersInterleaved(headers);
                            }
                            else
                            {
                                BufferInfoHeader::ComputeAccessorsHeadersPlanar(headers);
                            }
                            BufferInfoHeader::WriteAccessorsHeaders(pData, headers);
                            pData += accessorsHeadersLength;
                        }

                        if (needsResampling){
#if ENABLE_AUDIO_RESAMPLING
                            if (useAccessorHeaders_){
                                setResamplerOutput(swrOut, pData, headers, 0);
                                if(swr_convert(swr, swrOut, dstSampleCapacity, (const uint8_t **)avFrame->data, avFrame->nb_samples) < 0){
                                    throw "resampling error";
                                }
                                dstSamplesWritten = dstSampleCapacity;
                            } else {
                                setResamplerOutput(swrOut, pData, &bufferInfo, dstSamplesWritten);
                                int numSamplesOut = swr_convert(swr, swrOut, dstSampleCapacity-dstSamplesWritten, (const uint8_t **)avFrame->data, avFrame->nb_samples);
                                if(numSamplesOut < 0){
                                    throw "resampling error";
                                }
                                dstSamplesWritten += numSamplesOut;
                            }
                            srcSamplesRead = -1;
                            av_frame_unref(avFrame);
#else
                            throw "resampling disabled";
#endif
                        } else {
                            if(useAccessorHeaders_){
                                copyAudioSamples(avFrame->data, 0, pData, 0, avFrame->nb_samples, outputSampleSize, outputNumChannels, !planarAudioFormat);
                                dstSamplesWritten = dstSampleCapacity;
                                av_frame_unref(avFrame);
                                srcSamplesRead = -1;
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

                        }
                        if(dstSamplesWritten == dstSampleCapacity){
                            totalWrittenSamples += dstSamplesWritten;
                            while (!quit_){
                                try {
                                    handler->writeFrame2(mafFrame);
                                    break;
                                } catch (...){
                                    continue;
                                }
                            }
                            mafFrame = nullptr;
                            dstSamplesWritten = 0;
                        }
                }
            }
        }
    }
    catch (const int &ex)
    {
        error_ = make_exception_ptr(std::runtime_error(std::to_string(ex)));
    }
    catch (const char *ex)
    {
        error_ = make_exception_ptr(std::runtime_error(std::string(ex)));
    }
    catch (...)
    {
        error_ = std::current_exception();
    }

#if ENABLE_AUDIO_RESAMPLING
    swr_free(&swr);
    delete[] swrOut;
#endif
    if (headers)
        delete headers;
}

void AudioDecoder::Start(MafBuffer *mafBuffer)
{
    if (IsRunning())
    {
        return;
    }
    quit_ = false;
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