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
    #define putenv _putenv_s
#endif

#include <filesystem>
#include <catch2/catch_test_macros.hpp>
#include <stdlib.h>
#include <maf.hpp>
#include <factory.hpp>

using namespace MAF;

// must be path to a directory containing a valid maf pipeline factory
auto MAF_PLUGINS_DIR = getenv("MAF_PLUGINS_DIR");
 
TEST_CASE("MediaPipelineFactory::loadPluginsDir(), given MAF_PLUGINS_DIR is not set"){
    MAF::MediaPipelineFactory factory;
    char env[] = "MAF_PLUGINS_DIR=";
    putenv(env);
    REQUIRE_NOTHROW(factory.loadPluginsDir());
    REQUIRE(factory.plugins.size() == 0);
}

TEST_CASE("MediaPipelineFactory::loadPluginsDir(), given MAF_PLUGINS_DIR is set but not valid"){
    MAF::MediaPipelineFactory factory;
    CHECK(!std::filesystem::directory_entry("/path/not/found").exists());
    char env[] = "MAF_PLUGINS_DIR=/path/not/found";
    putenv(env);
    REQUIRE_NOTHROW(factory.loadPluginsDir());
    REQUIRE(factory.plugins.size() == 0);
}

/*
TEST_CASE("MediaPipelineFactory::loadPluginsDir(), given MAF_PLUGINS_DIR contains plugins"){
    MAF::MediaPipelineFactory factory;
    if (MAF_PLUGINS_DIR == 0){
        SKIP("MAF_PLUGINS_DIR environment variable not set");
    }
    CHECK(std::filesystem::directory_entry(MAF_PLUGINS_DIR).exists());
    putenv("MAF_PLUGINS_DIR", MAF_PLUGINS_DIR);
    REQUIRE_NOTHROW(factory.loadPluginsDir());
    REQUIRE(factory.plugins.size() > 0);
    for (auto plugin : factory.plugins){
        IMediaPipeline* tmp = plugin->createMediaPipeline();
        REQUIRE(tmp != nullptr);
        delete tmp;
    }
}
*/
