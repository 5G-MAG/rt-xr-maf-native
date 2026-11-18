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

// The graphics APIs of the plugin

#pragma once

#include <memory>

#include "IUnityInterface.h"
#include "TextureRegistry.hpp"

class GraphicsBackend
{
public:
    virtual ~GraphicsBackend() = default;
    virtual PictureImporter &Importer() = 0;
    // MAFUNITY_EVENT_RELEASE, render thread: the frames retired so far are released once the GPU completed the commands
    // recorded until now
    virtual void Release(TextureRegistry &registry) = 0;
    // MAFUNITY_EVENT_UPDATE, render thread: renders the frame the texture shows into its target (graphics APIs without
    // plane textures)
    virtual void Update(TextureRegistry &registry, int texture)
    {
        (void)registry;
        (void)texture;
    }
};

#ifdef __APPLE__
// Metal, when Unity uses it; nullptr otherwise
std::unique_ptr<GraphicsBackend> CreateMetalBackend(IUnityInterfaces *interfaces);
#endif

#ifdef __ANDROID__
// Vulkan, when Unity uses it and its device can import AHardwareBuffers; nullptr otherwise
std::unique_ptr<GraphicsBackend> CreateVulkanBackend(IUnityInterfaces *interfaces);
// Enables the device extensions the plugin needs when Unity creates its Vulkan device: call when the plugin is loaded,
// before the graphics device is created (the plugin must be loaded at startup)
void InterceptVulkanInitialization(IUnityInterfaces *interfaces);
#endif
