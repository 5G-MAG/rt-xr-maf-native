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

#include<filesystem>
namespace fs = std::filesystem;

namespace MAF {

IMediaPipeline* MediaPipelineFactory::createMediaPipeline(MediaInfo mediaInfo, std::vector<BufferInfo> buffers){
        IMediaPipeline* res = nullptr;
        for (auto & plugin : plugins ){      
            try {
                res = plugin->createMediaPipeline();
                res->initialize(mediaInfo, buffers);
                return res;
            } catch (...){
                delete res;
                res = nullptr;
            }
        } 
        return res;
}

void MediaPipelineFactory::loadPluginsDir(char* plugins_dir_cstr){

        if (plugins_dir_cstr == nullptr){
            plugins_dir_cstr = getenv("MAF_PLUGINS_DIR");
        }

        if (plugins_dir_cstr != NULL){
        
            fs::path plugins_dir = plugins_dir_cstr;

#ifdef _WIN32
            SetDllDirectory(plugins_dir_cstr);
            const char* DL_EXT = ".dll";
#else
            const char* DL_EXT = ".so";
#endif

            if (!fs::is_directory(plugins_dir)){
                std::cerr << "env. MAF_PLUGINS_DIR : not a valid directory" << std::endl;
                return;
            }

            for (auto const& entry : fs::directory_iterator(plugins_dir)) 
            {
                auto p = entry.path();
                if (entry.is_regular_file() && p.has_extension() && (p.extension() == DL_EXT)){
                    try {
                        loadPluginDL(const_cast<char*>(p.string().c_str()));
                    } catch(const std::exception& e) {
                        std::cerr << "Failed to load MAF plugin - " << e.what() << std::endl;
                    }
                }
            }

        } else {
            std::cerr << "enviornment variable not set: MAF_PLUGINS_DIR" << std::endl;
        }
}


void MediaPipelineFactory::loadPluginDL(char* dll){

    std::filesystem::path fp = dll;
    DL_HANDLE handle_ = NULL;

    std::filesystem::directory_entry entry{fp};
    if (!entry.exists()){
        throw "Not found: " + fp.string();
    }

#ifdef _WIN32
    handle_ = LoadLibraryW((LPCWSTR)fp.wstring().c_str()); // Will fail on character encoding conversion. 
    if (!handle_){
        auto err = GetLastError();
        if (err == ERROR_MOD_NOT_FOUND){
                throw "The plugin or one of its dependency was not found: " + fp.string();
        }
        if (err == ERROR_BAD_EXE_FORMAT ){
                throw "Invalid DLL format: " + fp.string();
        }
        throw "Failed to load DLL - unknown error: " + fp.string();
    }
#else
    handle_ = dlopen(lib, RTLD_LAZY | RTLD_DEEPBIND);
    if (!handle_){
        throw "Failed to load plugin library: " + fp.string();
    }
#endif

    if (handle_){
#ifdef _WIN32
    /* GCC 8 adds -Wcast-function-type, enabled by default with -Wextra, which
    causes this line to emit a warning on MinGW. We know what we're doing,
    so suppress that. */
    #if __GNUC__ >= 8
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-function-type"
    #endif
        RegisterFactoryPluginFn fpRegisterFactoryPlugin = reinterpret_cast<RegisterFactoryPluginFn>(GetProcAddress(handle_, fpFnName.c_str()));
    #if __GNUC__ >= 8
    #pragma GCC diagnostic pop
    #endif
#else
        RegisterFactoryPluginFn fpRegisterFactoryPlugin = reinterpret_cast<RegisterFactoryPluginFn>(dlsym(handle_, fpFnName));
#endif
        fpRegisterFactoryPlugin(this);

        handles_.push_back(handle_);

    }
    
    return;
}


MediaPipelineFactory::~MediaPipelineFactory(){
    for (auto plugin : plugins){
        delete plugin;
    }
    plugins.clear();
    for (auto handle_ : handles_){
#ifdef _WIN32
        FreeLibrary(handle_);
#else
        dlclose(handle_);
#endif
    }
    handles_.clear();
};



} // namespace MAF