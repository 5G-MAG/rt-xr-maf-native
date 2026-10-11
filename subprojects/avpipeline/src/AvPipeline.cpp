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


#include <regex>
#include <map>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <chrono>

#include <AvPipeline.hpp>

#ifdef __ANDROID__
#include <cstdio>
#include <cstdarg>
// Debug log of the media pipeline: stdout does not reach logcat, so lines go to a file in the app's data folder.
static void xrLog(const char* fmt, ...)
{
    FILE* f = fopen("/sdcard/Android/data/com.fivegmag.rtxrplayer/files/avpipeline.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}
#else
#define xrLog(...) do {} while (0)
#endif
#include <maf.hpp>

extern "C" {

#ifdef _MSC_VER
    #pragma warning(push) 
    #pragma warning(disable: 4244)
#endif
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
#ifdef _MSC_VER
    #pragma warning(default: 4244)
#endif    

};

using namespace MAF;

AvMediaPipeline::AvMediaPipeline(): state_(PipelineState::IDLE), stateLock_(new std::mutex()), avFmtCtx_(nullptr) {};

AvMediaPipeline::~AvMediaPipeline(){
    for(auto &[streamId, dec] : decoders_){
        if(dec){
            dec->Stop();
        }
    }
    quit_ = true;

    if(thread_.joinable()){
        thread_.join();
    }

    for(auto &[streamId, dec] : decoders_){
        if(dec){
            delete dec;
        }
    }
    delete stateLock_;
};

static int avStreamIdxFromTrackString(AVFormatContext* avFmtCtx, std::string s){
    // try parsing Track.track and returns a libav stream index for the track
    // this should be processing the string depending on the media format (dash, rtp ...)
    // for now this assumes ISOBMFF w/ trackID > 0: "#track_ID=N" (ISO/IEC 14496-12 Annex C was not opened: unverified,
    // "#track=N" is read the same way), N is the track_ID, which libav keeps in AVStream::id
    std::regex r("#track(?:_ID)?=([0-9]+)");
    std::smatch m;
    if (std::regex_search(s, m, r) == true) {
        int trackId = std::stoi( m[1].str().c_str() );
        for (unsigned i = 0; i < avFmtCtx->nb_streams; i++){
            if (avFmtCtx->streams[i]->id == trackId) return (int)i;
        }
        return trackId - 1;
    }
    return -1;
}

static void createDecoders(AVFormatContext* avFmtCtx, std::map<uint32_t, MafBuffer>* streamBufferMap, std::map<uint32_t, Decoder*>* decoders){
    for(auto & [streamIdx, _] : *streamBufferMap){
        Decoder* dec = CreateDecoder(avFmtCtx, streamIdx);
        if (dec){
            (*decoders)[streamIdx] = dec;
        } else {
            throw "Failed creating decoders";
        }
    }
}

static bool compareBufferIds(MafBuffer* &a, MafBuffer* &b){
    return a->second[0].bufferId < b->second[0].bufferId;
}

static bool isMetadataAttribute(AttributeType a){
    return a == AttributeType::METADATA_ACCESSOR || a == AttributeType::METADATA_SCENE_UPDATE;
}

static void mapTracksToMafBuffers(AVFormatContext* avFmtCtx, std::map<uint32_t, MafBuffer> &streamBufferMap, std::vector<BufferInfo> buffers, AlternativeLocation* alt){
    /* 
    Helper function to map tracks to buffers.
    - mapping is done according to AlternativeLocation.tracks if it is defined, in that case only buffers explicitly referenced by tracks are mapped
    - if AlternativeLocation.tracks is undefined, buffers are sorted based on bufferId and mapped to tracks in increasing order;
      buffers of timed metadata (METADATA_*) take the metadata (DATA) streams, the other buffers the remaining streams
    - throws if buffers is not consistent, or if not all buffers could be mapped to a track.
    */
    std::map<int, MafBuffer> idMafBuffer;
    std::vector<MafBuffer*> sorted;

    for (BufferInfo buf : buffers){
        if (idMafBuffer[buf.bufferId].first == nullptr){
            idMafBuffer[buf.bufferId].first = buf.handler;
            sorted.push_back(&idMafBuffer[buf.bufferId]);
        } else if (idMafBuffer[buf.bufferId].first != buf.handler) {
            throw "invalid MAF buffer configuration";
        }
        idMafBuffer[buf.bufferId].second.push_back(buf);
    }
    if (alt->tracks.size() == 0){
        // map buffer to streams in increasing order
        sort(sorted.begin(), sorted.end(), compareBufferIds);
        // a buffer takes the next stream of its own kind: metadata buffers the DATA streams, texture buffers the video
        // streams, the other (audio) buffers the audio streams; a kind without a stream of its own takes the other media streams
        std::vector<uint32_t> metadataStreams, videoStreams, audioStreams, otherStreams;
        for (uint32_t s = 0; s < avFmtCtx->nb_streams; s++){
            switch (avFmtCtx->streams[s]->codecpar->codec_type){
                case AVMEDIA_TYPE_DATA: metadataStreams.push_back(s); break;
                case AVMEDIA_TYPE_VIDEO: videoStreams.push_back(s); break;
                case AVMEDIA_TYPE_AUDIO: audioStreams.push_back(s); break;
                default: otherStreams.push_back(s); break;
            }
        }
        size_t nextMetadata = 0, nextVideo = 0, nextAudio = 0, nextOther = 0;
        for(size_t i=0; i < sorted.size(); i++ ){
            AttributeType at = sorted[i]->second[0].attributeType;
            bool metadata = isMetadataAttribute(at);
            bool texture = at >= AttributeType::TEXTURE_RGB && at <= AttributeType::TEXTURE_DEPTH_COMPONENT;
            std::vector<uint32_t>& pool = metadata ? metadataStreams : (texture ? videoStreams : audioStreams);
            size_t& next = metadata ? nextMetadata : (texture ? nextVideo : nextAudio);
            uint32_t chosen;
            if (next < pool.size()){
                chosen = pool[next++];
            } else if (!metadata && nextOther < otherStreams.size()){
                chosen = otherStreams[nextOther++];
            } else {
                throw "failed mapping media tracks to maf buffers";
            }
            streamBufferMap[chosen] = *(sorted[i]);
        }
    } else {
        // try using alternative.tracks.bufferId
        for (Track track : alt->tracks) {
            int id = avStreamIdxFromTrackString(avFmtCtx, track.track);
            if (id < 0){
                id = track.id; // unsure about this
            }
            if(streamBufferMap.find(id) != streamBufferMap.end()){
                throw "not implemented";
            }
            auto it = idMafBuffer.find(track.bufferId);
            if (it != idMafBuffer.end()){
                streamBufferMap[id] = (*it).second;
                idMafBuffer.erase(it); // not implemented: decoding same stream to 2 different buffers
            } else {
                throw "error while processing track.bufferId";
            }
        }
    }

}

void AvMediaPipeline::initialize(MediaInfo mediaInfo, std::vector<BufferInfo> buffers){
    /*
    The Presentation Engine initializes a new media pipeline. 
    It provides information related to the requested media or metadata. 
    The MAF will setup the media pipeline and allocate the buffers, 
        if they have not been allocated by the Presentation Engine.
    */
    mediaInfo_ = mediaInfo;
    buffer_info_ = buffers;

    try {

        for (BufferInfo buff : buffer_info_) {
            if(!buff.handler || (buff.handler->capacity() < 2)){
                throw "BufferHandler must be allocated by presentation engine with a minimum capacity of 2 frames.";
            }
        }

#ifdef __ANDROID__
        // FFmpeg warnings and errors (demuxer, network) go to the same debug file
        av_log_set_callback([](void*, int level, const char* fmt, va_list vl) {
            if (level > AV_LOG_WARNING) return;
            FILE* f = fopen("/sdcard/Android/data/com.fivegmag.rtxrplayer/files/avpipeline.log", "a");
            if (!f) return;
            fputs("[ffmpeg] ", f); vfprintf(f, fmt, vl); fclose(f);
        });
#endif
        AlternativeLocation* selected = nullptr;
        for (auto &alt : mediaInfo.alternatives) {
            if (avformat_open_input(&avFmtCtx_, alt.uri.c_str(), NULL, NULL) == 0) {
                selected = &alt;
                break;
            }
        }

        if (!selected){
            throw "failed to open media";
        }
        if (avformat_find_stream_info(avFmtCtx_, NULL) < 0) {
            throw "could not find stream information";
        }
        xrLog("opened %s: %u stream(s)", selected->uri.c_str(), avFmtCtx_->nb_streams);
        for (unsigned i = 0; i < avFmtCtx_->nb_streams; i++) {
            xrLog("  stream %u: type %d codec %d start %.3f", i, (int)avFmtCtx_->streams[i]->codecpar->codec_type, (int)avFmtCtx_->streams[i]->codecpar->codec_id, avFmtCtx_->streams[i]->start_time == AV_NOPTS_VALUE ? -1.0 : avFmtCtx_->streams[i]->start_time * av_q2d(avFmtCtx_->streams[i]->time_base));
        }
        
        mapTracksToMafBuffers(avFmtCtx_, this->streamBuffers_, buffers, selected);
        for (auto &[streamIdx, mafBuffer] : streamBuffers_){
            if (isMetadataAttribute(mafBuffer.second[0].attributeType) && streamIdx < avFmtCtx_->nb_streams && avFmtCtx_->streams[streamIdx]->codecpar->codec_type != AVMEDIA_TYPE_DATA){
                throw "a timed metadata buffer is mapped to a track that is not a metadata track";
            }
        }

        if (streamBuffers_.size() == 0){
            throw "failed mapping media tracks to maf buffers";
        }
        createDecoders(avFmtCtx_, &streamBuffers_, &decoders_);
        if (streamBuffers_.size() != decoders_.size()){
            throw "failed creating decoders";
        }
        stateLock_->lock();
        state_ = PipelineState::READY;
        stateLock_->unlock();
        
        return;

    } catch (const char* e) {
        std::cout << "Pipeline initialization error:" << e << std::endl;
        xrLog("pipeline initialization error: %s", e);
    } catch(const std::exception& e) {
        std::cout << "Pipeline initialization error:" << e.what() << std::endl;
        xrLog("pipeline initialization error: %s", e.what());
    }

#undef ERROR
    stateLock_->lock();
    state_ = PipelineState::ERROR;
    stateLock_->unlock();

};


void AvMediaPipeline::fetch(){

    AVPacket* avPkt = av_packet_alloc();
    
    try {

        /*
        Timeline of the generated media. Frame timestamps are source times (seconds from the start of the source, which is
        the container start time) minus the start offset, plus the duration of the passes already played when the media loops,
        so they start at 0 and keep increasing across loops.
        */
        const double origin = (avFmtCtx_->start_time != AV_NOPTS_VALUE) ? (avFmtCtx_->start_time / (double)AV_TIME_BASE) : 0.0;
        const double startOffset = timeInfo_.timeOffset > 0.0 ? timeInfo_.timeOffset : 0.0;
        const double startAbs = origin + startOffset;
        double endAbs = -1.0;
        if (timeInfo_.endTimeOffset >= 0.0) {
            if (timeInfo_.endTimeOffset > startOffset) {
                endAbs = origin + timeInfo_.endTimeOffset;
            } else {
                std::cout << "endTimeOffset (" << timeInfo_.endTimeOffset << " s) is not after startTimeOffset (" << startOffset << " s): ignored" << std::endl;
            }
        }

        for (auto & [streamIdx, dec]: decoders_){
            dec->SetRange(startAbs, endAbs, startOffset > 0.0);
        }

        if (startOffset > 0.0) {
            if (av_seek_frame(avFmtCtx_, -1, (int64_t)(startAbs * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD) < 0) {
                std::cout << "could not seek to startTimeOffset, frames before it are decoded and dropped" << std::endl;
            }
        }

        for (auto & [streamIdx, dec]: decoders_){
            dec->Start(&streamBuffers_[streamIdx]);
        }

        // limit of packets waiting in a decoder before a looping source is read again
        const size_t loopQueueLimit = 1000;
        std::map<int, bool> ended;       // streams that reached endTimeOffset in the current pass
        std::map<int, double> passEnd;   // end time of the last packet queued for a stream in the current pass
        double base = 0.0;
        bool finished = false;
        long long packetCount = 0;
        const size_t maxQueuedPackets = 400;
        
        while (!quit_)
        {
            for (auto & [streamIdx, dec]: decoders_){
                if (dec && dec->HasError()){
                    throw "a decoder failed";
                }
            }

            // Do not read faster than the decoders consume: a long stream would otherwise be downloaded and queued
            // whole, within seconds. A decoder holding this many packets is far enough ahead of the presentation.
            {
                bool decodersBusy = false;
                for (auto & [streamIdx, dec]: decoders_){
                    if (dec && dec->QueuedPackets() > maxQueuedPackets) {
                        decodersBusy = true;
                    }
                }
                if (decodersBusy) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }
            }

            bool passOver = false;
            int readResult = av_read_frame(avFmtCtx_, avPkt);
            if (readResult < 0){ // TODO: throttling
                char errText[128]; av_strerror(readResult, errText, sizeof errText);
                xrLog("read ended: %d (%s) after %lld packets", readResult, errText, (long long)packetCount);
                passOver = true;
            } else {
                if ((++packetCount % 500) == 1) xrLog("packet %lld stream %d pts %lld", (long long)packetCount, avPkt->stream_index, (long long)avPkt->pts);
                auto it = decoders_.find(avPkt->stream_index);
                if (it == decoders_.end()) {
                    av_packet_unref(avPkt);
                    continue;
                }
                AVStream* st = avFmtCtx_->streams[avPkt->stream_index];
                double tb = av_q2d(st->time_base);
                int64_t ts = avPkt->dts != AV_NOPTS_VALUE ? avPkt->dts : avPkt->pts;
                if (endAbs >= 0.0 && ts != AV_NOPTS_VALUE && ts * tb >= endAbs) {
                    // nothing of this stream before endTimeOffset follows
                    ended[avPkt->stream_index] = true;
                    av_packet_unref(avPkt);
                    passOver = (ended.size() == decoders_.size());
                } else {
                    if (avPkt->pts != AV_NOPTS_VALUE) {
                        double e = (avPkt->pts + (avPkt->duration > 0 ? avPkt->duration : 0)) * tb;
                        if (e > passEnd[avPkt->stream_index]) {
                            passEnd[avPkt->stream_index] = e;
                        }
                    }
                    it->second->QueuePacket(avPkt);
                }
            }
            if (!passOver) {
                continue;
            }

            // end of the source, or end of the generated range
            if (!timeInfo_.loop) {
                for (auto & [streamIdx, dec]: decoders_){
                    dec->QueueEnd();
                }
                finished = true;
                break;
            }
            double passStop = endAbs;
            if (passStop < 0.0) {
                for (auto & [streamIdx, e]: passEnd){
                    passStop = std::max(passStop, e);
                }
            }
            double duration = passStop - startAbs;
            if (duration <= 0.0) {
                std::cout << "can not loop: the media has no duration" << std::endl;
                for (auto & [streamIdx, dec]: decoders_){
                    dec->QueueEnd();
                }
                finished = true;
                break;
            }
            base += duration;
            for (auto & [streamIdx, dec]: decoders_){
                dec->QueueLoop(base);
            }
            if (av_seek_frame(avFmtCtx_, -1, (int64_t)(startAbs * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD) < 0) {
                std::cout << "can not loop: seeking to the start failed" << std::endl;
                for (auto & [streamIdx, dec]: decoders_){
                    dec->QueueEnd();
                }
                finished = true;
                break;
            }
            ended.clear();
            passEnd.clear();

            // do not queue passes faster than they are consumed
            bool waiting = true;
            while (waiting && !quit_) {
                waiting = false;
                for (auto & [streamIdx, dec]: decoders_){
                    if (dec && dec->QueuedPackets() > loopQueueLimit) {
                        waiting = true;
                    }
                }
                if (waiting) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }

        if (finished) {
            // the pipeline goes IDLE when every decoder has output its last frame
            bool allDone = false;
            while (!quit_ && !allDone) {
                allDone = true;
                for (auto & [streamIdx, dec]: decoders_){
                    if (dec && dec->HasError()){
                        throw "a decoder failed";
                    }
                    if (dec && !dec->IsDone()){
                        allDone = false;
                    }
                }
                if (!allDone) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
        }
        
        av_packet_free(&avPkt);
        stateLock_->lock();
        state_ = PipelineState::IDLE; // TODO: reset/resume/notify
        stateLock_->unlock();
        return;

    } catch (const int& ex) {
        xrLog("fetch error: %d", ex);
        error_ = std::make_exception_ptr(std::runtime_error(std::to_string(ex)));
    } catch (const char* ex) {
        xrLog("fetch error: %s", ex);
        error_ = std::make_exception_ptr(std::runtime_error(std::string(ex)));
    } catch (...) {
        error_ = std::current_exception();
    }
    

    av_packet_free(&avPkt);
    
    stateLock_->lock();
    state_ = PipelineState::ERROR; // TODO: reset/resume/notify
    stateLock_->unlock();

};

PipelineState AvMediaPipeline::state(){
    return state_;
};

BufferInfo* AvMediaPipeline::bufferInfo(int idx){
    return &buffer_info_[idx];
};

void AvMediaPipeline::setStateChangeCallback(void (*onStateChange)()){
    std::lock_guard<std::mutex> lock(*stateLock_);
    if(state_ != PipelineState::IDLE){
        throw std::runtime_error("can only set callback while state is IDLE");
    }
    onStateChange_ = onStateChange;
};

void AvMediaPipeline::updateView(ViewInfo viewInfo){
    viewInfo_ = viewInfo;
};

void AvMediaPipeline::startFetching(TimeInfo timeInfo, ViewInfo viewInfo){
    std::lock_guard<std::mutex> lock(*stateLock_);
    if(state_ != PipelineState::READY){
        return;
    }
    timeInfo_ = timeInfo;
    viewInfo_ = viewInfo;
    state_ = PipelineState::ACTIVE;
    thread_ = std::thread(&AvMediaPipeline::fetch, this);
    // call onStateChange()
};

void AvMediaPipeline::stopFetching(){
    stateLock_->lock();
    if(state_ != PipelineState::ACTIVE){
        stateLock_->unlock();
        return;
    }
    quit_ = true;
    for (auto & [streamIdx, dec]: decoders_){
        dec->Stop();
    }
    stateLock_->unlock();
    if (thread_.joinable()){
        thread_.join();
    }
};
