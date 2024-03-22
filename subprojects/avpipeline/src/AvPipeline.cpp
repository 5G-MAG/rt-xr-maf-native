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

#include <AvPipeline.hpp>
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

AvMediaPipeline::AvMediaPipeline(): stateLock_(new std::mutex()), state_(PipelineState::IDLE), avFmtCtx_(nullptr) {};

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

static int avStreamIdxFromTrackString(std::string s){
    // try parsing Track.track and returns a libav stream index for the track
    // this should be processing the string depending on the media format (dash, rtp ...)
    // for now this assumes ISOBMFF w/ trackID > 0
    std::regex r("#track_ID=([0-9]*)");
    std::smatch m; 
    if (std::regex_search(s, m, r) == true) {
        return std::stoi( m[1].str().c_str() ) - 1;
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

static void mapTracksToMafBuffers(std::map<uint32_t, MafBuffer> &streamBufferMap, std::vector<BufferInfo> buffers, AlternativeLocation* alt){
    /* 
    Helper function to map tracks to buffers.
    - mapping is done according to AlternativeLocation.tracks if it is defined, in that case only buffers explicitly referenced by tracks are mapped
    - if AlternativeLocation.tracks is undefined, buffers are sorted based on bufferId and mapped to tracks in increasing order
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
        for(int i=0; i < sorted.size(); i++ ){
            streamBufferMap[i] = *(sorted[i]);
        }
    } else {
        // try using alternative.tracks.bufferId
        for (Track track : alt->tracks) {
            int id = avStreamIdxFromTrackString(track.track);
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
        
        mapTracksToMafBuffers(this->streamBuffers_, buffers, selected);

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
    } catch(const std::exception& e) {
        std::cout << "Pipeline initialization error:" << e.what() << std::endl;
    }

#undef ERROR
    stateLock_->lock();
    state_ = PipelineState::ERROR;
    stateLock_->unlock();

};


void AvMediaPipeline::fetch(){

    AVPacket* avPkt = av_packet_alloc();
    
    try {
        
        for (auto & [streamIdx, dec]: decoders_){
            dec->Start(&streamBuffers_[streamIdx]);
        }
        
        while (!quit_)
        {
            if (av_read_frame(avFmtCtx_, avPkt) < 0){ // TODO: throttling
                bool running = false; // keep pipeline active as long as a decoder is running
                for (auto & [streamIdx, dec]: decoders_){
                    running = running || (dec && dec->IsRunning());
                }
                if(running){
                    continue;
                } else {
                    break;
                }
            }
            if (decoders_.find(avPkt->stream_index) != decoders_.end()){
                decoders_[avPkt->stream_index]->QueuePacket(avPkt);
            }
        }
        
        stateLock_->lock();
        state_ = PipelineState::IDLE; // TODO: reset/resume/notify
        stateLock_->unlock();
        return;

    } catch (const int& ex) {
        error_ = std::make_exception_ptr(std::runtime_error(std::to_string(ex)));
    } catch (const char* ex) {
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

__declspec(dllexport) void RegisterFactoryPlugin(MediaPipelineFactory* factory){
    factory->plugins.push_back((IMediaPipelineFactoryPlugin*)(new MediaPipelineFactoryPlugin<AvMediaPipeline>()));
}
