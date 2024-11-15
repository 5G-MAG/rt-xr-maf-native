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


#ifndef _MAF_FACTORY_
#define _MAF_FACTORY_

#include<string>
#include<map>
#include<functional>

#include<maf.hpp>

namespace MAF {

class MediaPipelineFactory {
    
    public:
        MediaPipelineFactory() = default;
        ~MediaPipelineFactory() = default;

        static MediaPipelineFactory& getInstance();
        
        std::map<std::string, std::function<IMediaPipeline*(void)>> registry;
        IMediaPipeline* createMediaPipeline(MediaInfo mediaInfo, std::vector<BufferInfo> buffers);
        void registerPlugin(std::string name, std::function<IMediaPipeline*(void)> factoryFn);

};

template <typename T> class MediaPipelineFactoryPlugin {
    public:
        MediaPipelineFactoryPlugin(std::string name){
            MediaPipelineFactory::getInstance().registerPlugin(name, [](void) -> IMediaPipeline * { return new T();});
        };
        ~MediaPipelineFactoryPlugin() = default;
};

}
#endif // _MAF_FACTORY_