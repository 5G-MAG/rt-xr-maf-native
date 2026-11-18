
> # Info
> This section is a *work in progress* intended to be contributed to the [XR section of 5G-MAG's Getting-Started website](https://5g-mag.github.io/Standards/pages/xr.html).


# Creating a custom MediaPipeline plugin for use in the XR Player

A media pipeline implements media fetching and decoding, exposing its functionalities through the [MAF API](./MAF.md).

## MediaPipeline project structure
```
- MediaPipeline
    - include
        MediaPipeline.h
    - src
        MediaPipeline.c
    meson.build
    README.md
```

## Implementing the IMediaPipeline interface

- review of the `IMediaPipeline` interface:
    - `include/MediaPipeline.h`
    - `src/MediaPipeline.cpp`

- Enable registration of the `MediaPipeline` as a plugin of the `MediaPipelineFactory`
- Register the plugin in the XR Player Unity project

## Compiling the MediaPipeline as a meson subproject
- `MediaPipeline/meson.build`
- `meson.build`
- `meson.options`



# maf_unity: showing native frames in Unity

`subprojects/maf_unity` is a Unity native rendering plugin that shows the frames of [native frame buffers](./maf.md#native-frame-buffers) in Unity textures without copying their pictures. Its C API is in [`MafUnity.h`](../subprojects/maf_unity/include/MafUnity.h).

For each texture, from the main thread:
1. `MafUnity_CreateTexture()` once, then `MafUnity_SetColorConversion()` with the `ycbcrModel` and `ycbcrRange` of the texture's `MPEG_sampler_YCbCr` (Vulkan enum values, -1 for the values the picture suggests). With Vulkan, `MafUnity_SetTargetTexture()` with the `RenderTexture` the frames are rendered into.
2. When a new frame of a native buffer is presented: `MafUnity_SubmitFrame(texture, handler, frame.index, frame.data, frame.width, frame.height)`. It imports the picture (once for a frame shown on several textures).
3. Metal: wrap the plane textures (`MafUnity_GetPlaneTexture`, with `MafUnity_GetPlaneFormat`, `MafUnity_GetPlaneWidth` and `MafUnity_GetPlaneHeight`) with `Texture2D.CreateExternalTexture` / `UpdateExternalTexture`, and render with them. Vulkan: `CommandBuffer.IssuePluginEventAndData(MafUnity_GetRenderEventFunc(), MAFUNITY_EVENT_UPDATE, texture)` before the commands that use the target texture: the frame is rendered into it, converted to RGB.
4. Each frame, after the commands that use the textures: `CommandBuffer.IssuePluginEventAndData(MafUnity_GetRenderEventFunc(), MAFUNITY_EVENT_RELEASE, IntPtr.Zero)`. The frames that no texture shows any more are released to their buffer (`BufferHandler::releaseFrame`) once the GPU has completed the commands recorded before the event.
5. `MafUnity_DestroyTexture()` when the texture is no longer used.

Frames read from a native buffer and not submitted (frames skipped to catch up with the presentation time) are released by the caller with `releaseFrame`. Before a buffer is freed, call `MafUnity_ForgetBuffer(handler)`.

| Graphics API | Native frames | Frames shown with |
|---|---|---|
| Metal (macOS, iOS) | `CVPixelBuffer` | an `id<MTLTexture>` per plane, without a copy |
| Vulkan (Android) | `AHardwareBuffer` | the target texture: the picture is imported as a Vulkan image without a copy, and rendered into the target through a `VkSamplerYcbcrConversion` (Unity can not sample the YCbCr formats of the device) |

With Vulkan, the plugin enables the extensions it needs (`VK_ANDROID_external_memory_android_hardware_buffer`, `VK_EXT_queue_family_foreign`, sampler YCbCr conversion) when Unity creates its device: the plugin must be loaded at startup (*Load on startup* in the plugin import settings). Otherwise `MafUnity_IsSupported()` returns 0 when the device lacks them. The shaders are in `subprojects/maf_unity/shaders`; `compile.sh` regenerates `src/VulkanShaders.h` with `glslc` from the Android NDK.

The plugin is built when the PluginAPI headers of a Unity installation are found: by default those of the newest Unity editor installed with Unity Hub, otherwise set with `-Dmaf_unity:unity_plugin_api=<Unity>/PluginAPI`. The headers are not part of this repository, as they are under the Unity Companion License. The top-level `unityplugin` option (`auto`, `enabled`, `disabled`) controls the build. The plugin is built for macOS and Android, with `libmaf` built with native frame buffers.
