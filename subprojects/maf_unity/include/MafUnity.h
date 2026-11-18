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
// - MafUnity_CreateTexture() once;
// - each time a new frame of a native buffer is presented: MafUnity_SubmitFrame(texture, handler, frame.index,
//   frame.data), then wrap the plane textures (MafUnity_GetPlaneTexture) with Texture2D.CreateExternalTexture /
//   UpdateExternalTexture and render with them;
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
    MAFUNITY_FORMAT_BGRA8 = 5  // 8-bit red, green, blue and alpha (TextureFormat.BGRA32)
};

// events of MafUnity_GetRenderEventFunc()
enum MafUnityEvent
{
    MAFUNITY_EVENT_RELEASE = 1
};

// Whether the plugin can import the native frames with the graphics API Unity uses
UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API MafUnity_IsSupported(void);

// A new texture: a positive identifier
UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API MafUnity_CreateTexture(void);

UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API MafUnity_DestroyTexture(int texture);

// Shows a frame of a native buffer on the texture: handler is the MAF::BufferHandler, frameIndex the index of the frame,
// picture its Frame.data. Returns the number of planes of the picture, 0 when it can not be imported.
UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API MafUnity_SubmitFrame(int texture, void *handler, int frameIndex, void *picture);

// The native texture of a plane of the frame the texture shows (id<MTLTexture> with Metal), or null
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
