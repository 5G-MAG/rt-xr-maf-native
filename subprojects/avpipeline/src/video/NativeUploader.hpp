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

// Native frames: pictures stored in memory the GPU imports (see BufferHandler::allocateNative in maf.hpp)

#pragma once

#include <cstdint>
#include <memory>

#include <maf.hpp>

// A picture in memory the GPU imports, the Frame.data of a native buffer: a CVPixelBufferRef on Apple platforms, an
// AHardwareBuffer* on Android. owner keeps the picture alive: the picture is released with the last copy of owner.
struct NativePicture
{
    void *handle = nullptr;
    std::shared_ptr<void> owner;
};

// The planes of a picture in system memory
struct PicturePlanes
{
    int count = 0;
    const uint8_t *data[3] = {nullptr, nullptr, nullptr};
    int stride[3] = {0, 0, 0};   // bytes from a row to the next
    int rowBytes[3] = {0, 0, 0}; // bytes of picture data in a row
    int rows[3] = {0, 0, 0};
};

// Stores pictures of system memory into native pictures
class NativeUploader
{
public:
    virtual ~NativeUploader() = default;
    // The format a picture requested in the given format is stored in: the same samples, in a layout the platform has
    // (e.g. the 2-plane layout of a YCbCr format without 3-plane native pictures). Throws when the platform can not
    // store the format natively.
    virtual MAF::ColorFormat StoredFormat(MAF::ColorFormat requested) = 0;
    // Copies a picture laid out in the stored format into a new native picture. fullRange: YCbCr samples use the
    // full range of their values (otherwise the video range). Returns false on failure.
    virtual bool Upload(const PicturePlanes &planes, MAF::ColorFormat stored, int width, int height, bool fullRange, NativePicture &out) = 0;
};

// The uploader of the platform, or nullptr when the platform has no native frames
std::unique_ptr<NativeUploader> CreateNativeUploader();
