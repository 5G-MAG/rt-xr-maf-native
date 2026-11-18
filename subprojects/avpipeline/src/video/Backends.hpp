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

// Backends available to CreateVideoBackend()

#pragma once

#include <VideoDecodeBackend.hpp>

// A libav decoder with a hardware acceleration of the given device type (VideoToolbox, VAAPI, D3D11VA...): the
// pictures are decoded in the memory of the device. Returns nullptr when the codec or the device does not support it.
// When the device fails to decode a picture, the decoder continues in software, unless required is set.
std::unique_ptr<VideoDecodeBackend> OpenHwAccelBackend(AVStream *stream, AVHWDeviceType type, bool required);

#ifdef __ANDROID__
// The number of images of the surface a MediaCodec decoder renders the pictures of the stream to, when they can be
// handed over as they are to the native frame buffer of info; 0 when the pictures are output in system memory.
int MediaCodecSurfaceImages(AVStream *stream, const MAF::BufferInfo &info);

// A MediaCodec decoder of the platform, through the NDK API, rendering to a surface of surfaceImages images (or output
// in system memory when 0). Returns nullptr when no MediaCodec decoder supports the stream.
std::unique_ptr<VideoDecodeBackend> OpenMediaCodecBackend(AVStream *stream, int surfaceImages);
#endif
