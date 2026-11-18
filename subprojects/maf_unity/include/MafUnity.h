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

// maf_unity: Unity native rendering plugin that shows the frames of MAF native frame buffers in Unity textures, without
// copying their pictures (see "Native frame buffers" in docs/maf.md).
//
// Usage, from the main thread of Unity, for each texture:
// - MafUnity_CreateTexture() once; MafUnity_SetColorConversion() with the MPEG_sampler_YCbCr of the texture; with
//   Vulkan, MafUnity_SetTargetTexture() with the texture the frames are rendered into;
// - each time a new frame of a native buffer is presented: MafUnity_SubmitFrame(texture, handler, frame.index,
//   frame.data, frame.width, frame.height), then
//   - Metal: wrap the plane textures (MafUnity_GetPlaneTexture) with Texture2D.CreateExternalTexture /
//     UpdateExternalTexture and render with them;
//   - Vulkan: CommandBuffer.IssuePluginEventAndData(MafUnity_GetRenderEventFunc(), MAFUNITY_EVENT_UPDATE, texture)
//     before the commands that use the target texture: the frame is rendered into it, converted to RGB;
// - each frame, after the commands that use the textures: CommandBuffer.IssuePluginEventAndData(
//   MafUnity_GetRenderEventFunc(), MAFUNITY_EVENT_RELEASE, IntPtr.Zero). The frames no texture shows any more are
//   released to their buffer (BufferHandler::releaseFrame) once the GPU completed the commands recorded before it;
// - MafUnity_DestroyTexture() when the texture is no longer used.
// Before a buffer is freed: MafUnity_ForgetBuffer(handler), so that its frames are no longer released to it.
// Frames that are read from a native buffer and not submitted (skipped frames) are released by the caller itself.

#pragma once

#include <stdint.h>

#include "IUnityGraphics.h"

#ifdef __cplusplus
extern "C" {
#endif

// format of a plane texture
enum MafUnityPlaneFormat
{
    MAFUNITY_FORMAT_NONE = 0,
    MAFUNITY_FORMAT_R8 = 1,    // one 8-bit component (Unity TextureFormat.R8)
    MAFUNITY_FORMAT_RG8 = 2,   // two 8-bit components (TextureFormat.RG16)
    MAFUNITY_FORMAT_R16 = 3,   // one 16-bit component (TextureFormat.R16)
    MAFUNITY_FORMAT_RG16 = 4,  // two 16-bit components (TextureFormat.RG32)
    MAFUNITY_FORMAT_BGRA8 = 5  // 8-bit blue, green, red and alpha (TextureFormat.BGRA32)
};

// events of MafUnity_GetRenderEventFunc()
enum MafUnityEvent
{
    // data: none. Releases the frames no texture shows, once the GPU completed the commands recorded before the event
    MAFUNITY_EVENT_RELEASE = 1,
    // data: the texture. Renders the frame the texture shows into its target texture (Vulkan)
    MAFUNITY_EVENT_UPDATE = 2
};

// Whether the plugin can import the native frames with the graphics API Unity uses
UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API MafUnity_IsSupported(void);

// A new texture: a positive identifier
UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API MafUnity_CreateTexture(void);

UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API MafUnity_DestroyTexture(int texture);

// The conversion of the YCbCr pictures to RGB (Vulkan): the ycbcrModel and ycbcrRange of MPEG_sampler_YCbCr, which are
// VkSamplerYcbcrModelConversion and VkSamplerYcbcrRange values; -1 uses the values the picture suggests. Applies to the
// frames submitted after the call. Returns 0 for an unknown texture.
UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API MafUnity_SetColorConversion(int texture, int ycbcrModel, int ycbcrRange);

// The texture the frames are rendered into by MAFUNITY_EVENT_UPDATE (Vulkan): Texture.GetNativeTexturePtr() of a
// RenderTexture; flipY: the picture is flipped vertically. Returns 0 for an unknown texture.
UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API MafUnity_SetTargetTexture(int texture, void *nativeTexture, int flipY);

// Shows a frame of a native buffer on the texture: handler is the MAF::BufferHandler, frameIndex the index of the frame,
// picture its Frame.data, width and height its Frame.width and height (the top-left region of the picture). Returns the
// number of plane textures of the picture (Metal), 1 when the frame is rendered into the target texture (Vulkan), 0
// when it can not be imported.
UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API MafUnity_SubmitFrame(int texture, void *handler, int frameIndex, void *picture, int width, int height);

// The native texture of a plane of the frame the texture shows (id<MTLTexture> with Metal), or null (Vulkan)
UNITY_INTERFACE_EXPORT void *UNITY_INTERFACE_API MafUnity_GetPlaneTexture(int texture, int plane);
// MafUnityPlaneFormat of a plane
UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API MafUnity_GetPlaneFormat(int texture, int plane);
UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API MafUnity_GetPlaneWidth(int texture, int plane);
UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API MafUnity_GetPlaneHeight(int texture, int plane);

// The frames of the buffer are no longer released to it: call before freeing the buffer
UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API MafUnity_ForgetBuffer(void *handler);

// Function for CommandBuffer.IssuePluginEventAndData, with a MafUnityEvent
UNITY_INTERFACE_EXPORT UnityRenderingEventAndData UNITY_INTERFACE_API MafUnity_GetRenderEventFunc(void);

#ifdef __cplusplus
}
#endif
