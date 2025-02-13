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


#ifndef _AV_MEDIA_PIPELINE_
#define _AV_MEDIA_PIPELINE_

#define STREAMID(trackIdx) (trackIdx-1)
#define TRACKID(streamIdx) (streamIdx+1)

#include <map>

#include <maf.hpp>
#include <factory.hpp>
#include <MafBuffer.hpp>
#include <Decoder.hpp>

extern "C" {

#ifdef _MSC_VER
    #pragma warning(push) 
    #pragma warning(disable : 4244)
#endif    
    #include <libavformat/avformat.h>
#ifdef _MSC_VER
    #pragma warning(default: 4244)
#endif

    using namespace MAF;
    
    class AvMediaPipeline : public IMediaPipeline
    {
    private:
        MediaInfo mediaInfo_;
        TimeInfo timeInfo_;
        ViewInfo viewInfo_;
        std::vector<BufferInfo> buffer_info_;
        PipelineState state_;
        void (*onStateChange_)();

        std::mutex* stateLock_;
        AVFormatContext* avFmtCtx_;
        std::map<uint32_t, MafBuffer> streamBuffers_;
        std::map<uint32_t, Decoder*> decoders_;
        std::thread thread_;
        std::exception_ptr error_;
        std::atomic<bool> quit_{false};
        void fetch();

    public:
        AvMediaPipeline();
        virtual ~AvMediaPipeline();
        virtual BufferInfo* bufferInfo(int idx);
        virtual PipelineState state();
        virtual void initialize(MediaInfo mediaInfo, std::vector<BufferInfo> buffers);
        virtual void setStateChangeCallback(void (*onStateChange)());
        virtual void startFetching(TimeInfo timeInfo, ViewInfo viewInfo);
        virtual void updateView(ViewInfo viewInfo);
        virtual void stopFetching();
    };

    MAF_REGISTER_PLUGIN_FN(AvMediaPipeline)

} // extern "C"


#endif