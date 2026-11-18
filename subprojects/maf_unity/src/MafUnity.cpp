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

// The Unity plugin: graphics device events, render thread events, and the C API of MafUnity.h

#include "MafUnity.h"

#include "IUnityInterface.h"
#include "IUnityGraphics.h"

#include "Backend.hpp"
#include "TextureRegistry.hpp"

namespace
{

    IUnityInterfaces *s_interfaces = nullptr;
    IUnityGraphics *s_graphics = nullptr;
    std::unique_ptr<GraphicsBackend> s_backend;

    TextureRegistry &Registry()
    {
        static TextureRegistry registry([](MAF::BufferHandler *handler, uint8_t index) { handler->releaseFrame(index); });
        return registry;
    }

    // The backend of the graphics API of Unity, or nullptr
    std::unique_ptr<GraphicsBackend> CreateBackend()
    {
#if defined(__APPLE__)
        return CreateMetalBackend(s_interfaces);
#elif defined(__ANDROID__)
        return CreateVulkanBackend(s_interfaces);
#else
        return nullptr;
#endif
    }

    void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType type)
    {
        if (type == kUnityGfxDeviceEventInitialize)
        {
            s_backend = CreateBackend();
        }
        else if (type == kUnityGfxDeviceEventShutdown)
        {
            s_backend.reset();
        }
    }

    void UNITY_INTERFACE_API OnRenderEvent(int eventId, void *data)
    {
        if (!s_backend)
        {
            return;
        }
        if (eventId == MAFUNITY_EVENT_RELEASE)
        {
            s_backend->Release(Registry());
        }
        else if (eventId == MAFUNITY_EVENT_UPDATE)
        {
            s_backend->Update(Registry(), (int)(intptr_t)data);
        }
    }

    const ImportedPicture *Current(int texture, int plane)
    {
        const ImportedPicture *p = Registry().Current(texture);
        return (p && plane >= 0 && plane < p->planeCount) ? p : nullptr;
    }

} // namespace

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *interfaces)
{
    s_interfaces = interfaces;
    s_graphics = interfaces->Get<IUnityGraphics>();
#ifdef __ANDROID__
    InterceptVulkanInitialization(interfaces);
#endif
    s_graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    // the graphics device may already be initialized
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
    if (s_graphics)
    {
        s_graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
    }
    s_backend.reset();
}

extern "C" {

int UNITY_INTERFACE_API MafUnity_IsSupported(void)
{
    return s_backend ? 1 : 0;
}

int UNITY_INTERFACE_API MafUnity_CreateTexture(void)
{
    return Registry().Create();
}

void UNITY_INTERFACE_API MafUnity_DestroyTexture(int texture)
{
    Registry().Destroy(texture);
}

int UNITY_INTERFACE_API MafUnity_SetColorConversion(int texture, int ycbcrModel, int ycbcrRange)
{
    TextureRegistry::Settings s = Registry().GetSettings(texture);
    s.ycbcrModel = ycbcrModel;
    s.ycbcrRange = ycbcrRange;
    return Registry().SetSettings(texture, s) ? 1 : 0;
}

int UNITY_INTERFACE_API MafUnity_SetTargetTexture(int texture, void *nativeTexture, int flipY)
{
    TextureRegistry::Settings s = Registry().GetSettings(texture);
    s.target = nativeTexture;
    s.flipY = flipY != 0;
    return Registry().SetSettings(texture, s) ? 1 : 0;
}

int UNITY_INTERFACE_API MafUnity_SubmitFrame(int texture, void *handler, int frameIndex, void *picture, int width, int height)
{
    if (!s_backend || frameIndex < 0 || frameIndex > 255)
    {
        return 0;
    }
    const ImportedPicture *p = Registry().Submit(texture, (MAF::BufferHandler *)handler, (uint8_t)frameIndex, picture, width, height, s_backend->Importer());
    if (!p)
    {
        return 0;
    }
    // a picture the backend renders into the target has no plane textures
    return p->planeCount > 0 ? p->planeCount : 1;
}

void *UNITY_INTERFACE_API MafUnity_GetPlaneTexture(int texture, int plane)
{
    const ImportedPicture *p = Current(texture, plane);
    return p ? p->texture[plane] : nullptr;
}

int UNITY_INTERFACE_API MafUnity_GetPlaneFormat(int texture, int plane)
{
    const ImportedPicture *p = Current(texture, plane);
    return p ? p->format[plane] : MAFUNITY_FORMAT_NONE;
}

int UNITY_INTERFACE_API MafUnity_GetPlaneWidth(int texture, int plane)
{
    const ImportedPicture *p = Current(texture, plane);
    return p ? p->width[plane] : 0;
}

int UNITY_INTERFACE_API MafUnity_GetPlaneHeight(int texture, int plane)
{
    const ImportedPicture *p = Current(texture, plane);
    return p ? p->height[plane] : 0;
}

void UNITY_INTERFACE_API MafUnity_ForgetBuffer(void *handler)
{
    Registry().ForgetBuffer((MAF::BufferHandler *)handler);
}

UnityRenderingEventAndData UNITY_INTERFACE_API MafUnity_GetRenderEventFunc(void)
{
    return OnRenderEvent;
}

} // extern "C"
