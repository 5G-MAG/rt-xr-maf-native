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


#ifdef _MSC_VER
    #ifdef _CRT_SECURE_NO_WARNINGS
        #undef _CRT_SECURE_NO_WARNINGS
    #endif
    #define _CRT_SECURE_NO_WARNINGS 1
#endif

#include<factory.hpp>

namespace MAF {

MediaPipelineFactory::FactoryMap& MediaPipelineFactory::GetRegistry(){
    static MediaPipelineFactory::FactoryMap registry;
    return registry;    
}

void MediaPipelineFactory::registerPlugin(std::string name, std::function<IMediaPipeline*(void)> factoryFn){
    auto registry = MediaPipelineFactory::GetRegistry();
    registry[name] = factoryFn;
}

IMediaPipeline* MediaPipelineFactory::createMediaPipeline(MediaInfo mediaInfo, std::vector<BufferInfo> buffers){
    IMediaPipeline* res = nullptr;
    for (auto &[name, factoryFn] : MediaPipelineFactory::GetRegistry() ){      
        try {
            res = factoryFn();
            res->initialize(mediaInfo, buffers);
            return res;
        } catch (...){
            delete res;
            res = nullptr;
        }
    }
    return res;
}

} // namespace MAF