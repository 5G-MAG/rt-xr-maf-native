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

// The frames the textures show, and their release once the GPU no longer uses them. Independent of the graphics API.

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

#include <maf.hpp>

// A picture imported for the GPU: one native texture of the graphics API per plane. Destroying it releases the
// textures and the reference to the picture the importer took.
struct ImportedPicture
{
    int planeCount = 0;
    void *texture[3] = {nullptr, nullptr, nullptr};
    int format[3] = {0, 0, 0}; // MafUnityPlaneFormat
    int width[3] = {0, 0, 0};
    int height[3] = {0, 0, 0};
    std::function<void()> release;
    ~ImportedPicture()
    {
        if (release)
        {
            release();
        }
    }
};

// Imports the pictures of native frames (Frame.data) for a graphics API
class PictureImporter
{
public:
    virtual ~PictureImporter() = default;
    // The picture imported, or nullptr when it can not be
    virtual std::shared_ptr<ImportedPicture> Import(void *picture) = 0;
};

class TextureRegistry
{
public:
    // releaseFrame: what releases a frame to its buffer (BufferHandler::releaseFrame)
    explicit TextureRegistry(std::function<void(MAF::BufferHandler *, uint8_t)> releaseFrame);

    int Create();
    void Destroy(int texture);
    // Shows a frame on a texture, importing its picture unless another texture shows it already. The frame the texture
    // showed before is retired when no texture shows it any more. Returns the picture, nullptr on failure.
    const ImportedPicture *Submit(int texture, MAF::BufferHandler *handler, uint8_t frameIndex, void *picture, PictureImporter &importer);
    // The picture the texture shows, or nullptr
    const ImportedPicture *Current(int texture);

    // The frames retired so far are released when Complete() is called with the batch returned: once the GPU completed
    // the commands recorded until now. Returns 0 when there is nothing to release.
    uint64_t RetireBatch();
    // Releases the frames of a batch (any thread)
    void Complete(uint64_t batch);
    // The frames of the buffer are no longer released to it (it is about to be freed)
    void ForgetBuffer(MAF::BufferHandler *handler);

    // frames shown, retired and pending in batches (for tests)
    size_t ShownFrames();
    size_t PendingFrames();

private:
    struct Shown
    {
        MAF::BufferHandler *handler;
        uint8_t index;
        void *picture;
        std::shared_ptr<ImportedPicture> imported;
        int users = 0;
    };
    using Key = std::tuple<MAF::BufferHandler *, uint8_t, void *>;

    void Retire(const Key &key); // requires mutex_

    std::function<void(MAF::BufferHandler *, uint8_t)> releaseFrame_;
    std::mutex mutex_;
    int nextTexture_ = 1;
    std::map<int, Key> textures_;          // texture -> frame it shows (handler nullptr: none)
    std::map<Key, Shown> shown_;
    std::vector<Shown> retired_;           // no texture shows them, waiting for a batch
    uint64_t nextBatch_ = 1;
    std::map<uint64_t, std::vector<Shown>> batches_;
};
