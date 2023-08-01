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

#include<string.h>

#include<filesystem>
#include<maf.hpp>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

namespace MAF {

#ifdef _WIN32
    typedef HMODULE DL_HANDLE;
#else
    typedef void* DL_HANDLE;
#endif

class IMediaPipelineFactoryPlugin
{ 
    public:
        virtual ~IMediaPipelineFactoryPlugin()=default;
        virtual IMediaPipeline* createMediaPipeline()=0;
};

template <typename T> class MediaPipelineFactoryPlugin : IMediaPipelineFactoryPlugin {
    public:
         MediaPipelineFactoryPlugin() = default;
        ~MediaPipelineFactoryPlugin() = default;

        IMediaPipeline* createMediaPipeline() {
            return new T();
        }
};

class MediaPipelineFactory {
    
    private:
        std::vector<DL_HANDLE> handles_; 

    public:
        MediaPipelineFactory() = default;
        ~MediaPipelineFactory();
        std::vector<IMediaPipelineFactoryPlugin*> plugins;
        void loadPluginsDir(char * plugin_dir = nullptr);
        void loadPluginDL(char * dll);
        IMediaPipeline* createMediaPipeline(MediaInfo mediaInfo, std::vector<BufferInfo> buffers);
};

typedef void (*RegisterFactoryPluginFn)(MediaPipelineFactory*);

const std::string fpFnName = "RegisterFactoryPlugin";

}
#endif // _MAF_FACTORY_