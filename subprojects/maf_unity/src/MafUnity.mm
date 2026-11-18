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
#import <Metal/Metal.h>
#include "IUnityGraphicsMetal.h"

#include "Importers.hpp"
#include "TextureRegistry.hpp"

namespace
{

    IUnityInterfaces *s_interfaces = nullptr;
    IUnityGraphics *s_graphics = nullptr;
    IUnityGraphicsMetalV1 *s_metal = nullptr;
    std::unique_ptr<PictureImporter> s_importer;

    TextureRegistry &Registry()
    {
        static TextureRegistry registry([](MAF::BufferHandler *handler, uint8_t index) { handler->releaseFrame(index); });
        return registry;
    }

    // An importer for the graphics API of Unity, or nullptr
    std::unique_ptr<PictureImporter> CreateImporter()
    {
        if (s_graphics && s_graphics->GetRenderer() == kUnityGfxRendererMetal)
        {
            s_metal = s_interfaces->Get<IUnityGraphicsMetalV1>();
            if (s_metal)
            {
                return CreateMetalImporter(s_metal->MetalDevice());
            }
        }
        return nullptr;
    }

    void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType type)
    {
        if (type == kUnityGfxDeviceEventInitialize)
        {
            s_importer = CreateImporter();
        }
        else if (type == kUnityGfxDeviceEventShutdown)
        {
            s_importer.reset();
            s_metal = nullptr;
        }
    }

    void UNITY_INTERFACE_API OnRenderEvent(int eventId, void *data)
    {
        (void)data;
        if (eventId != MAFUNITY_EVENT_RELEASE)
        {
            return;
        }
        const uint64_t batch = Registry().RetireBatch();
        if (batch == 0)
        {
            return;
        }
        // the frames retired before the event are no longer used once the commands recorded until now complete
        id<MTLCommandBuffer> commands = s_metal ? s_metal->CurrentCommandBuffer() : nil;
        if (commands)
        {
            [commands addCompletedHandler:^(id<MTLCommandBuffer>) {
              Registry().Complete(batch);
            }];
        }
        else
        {
            Registry().Complete(batch);
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
    s_importer.reset();
}

extern "C" {

int UNITY_INTERFACE_API MafUnity_IsSupported(void)
{
    return s_importer ? 1 : 0;
}

int UNITY_INTERFACE_API MafUnity_CreateTexture(void)
{
    return Registry().Create();
}

void UNITY_INTERFACE_API MafUnity_DestroyTexture(int texture)
{
    Registry().Destroy(texture);
}

int UNITY_INTERFACE_API MafUnity_SubmitFrame(int texture, void *handler, int frameIndex, void *picture)
{
    if (!s_importer || frameIndex < 0 || frameIndex > 255)
    {
        return 0;
    }
    const ImportedPicture *p = Registry().Submit(texture, (MAF::BufferHandler *)handler, (uint8_t)frameIndex, picture, *s_importer);
    return p ? p->planeCount : 0;
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
