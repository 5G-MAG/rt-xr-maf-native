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

#include "TextureRegistry.hpp"

TextureRegistry::TextureRegistry(std::function<void(MAF::BufferHandler *, uint8_t)> releaseFrame)
    : releaseFrame_(std::move(releaseFrame))
{
}

int TextureRegistry::Create()
{
    std::lock_guard<std::mutex> lock(mutex_);
    int id = nextTexture_++;
    textures_[id] = Key(nullptr, 0, nullptr);
    return id;
}

void TextureRegistry::Retire(const Key &key)
{
    auto it = shown_.find(key);
    if (it == shown_.end())
    {
        return;
    }
    if (--it->second.users > 0)
    {
        return;
    }
    retired_.push_back(std::move(it->second));
    shown_.erase(it);
}

void TextureRegistry::Destroy(int texture)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = textures_.find(texture);
    if (it == textures_.end())
    {
        return;
    }
    Retire(it->second);
    textures_.erase(it);
}

const ImportedPicture *TextureRegistry::Submit(int texture, MAF::BufferHandler *handler, uint8_t frameIndex, void *picture, PictureImporter &importer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto t = textures_.find(texture);
    if (t == textures_.end() || handler == nullptr || picture == nullptr)
    {
        return nullptr;
    }
    const Key key(handler, frameIndex, picture);
    if (t->second == key)
    {
        return shown_[key].imported.get();
    }
    auto s = shown_.find(key);
    if (s == shown_.end())
    {
        std::shared_ptr<ImportedPicture> imported = importer.Import(picture);
        if (!imported)
        {
            return nullptr;
        }
        s = shown_.emplace(key, Shown{handler, frameIndex, picture, std::move(imported), 0}).first;
    }
    s->second.users++;
    Retire(t->second);
    t->second = key;
    return s->second.imported.get();
}

const ImportedPicture *TextureRegistry::Current(int texture)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto t = textures_.find(texture);
    if (t == textures_.end())
    {
        return nullptr;
    }
    auto s = shown_.find(t->second);
    return s == shown_.end() ? nullptr : s->second.imported.get();
}

uint64_t TextureRegistry::RetireBatch()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (retired_.empty())
    {
        return 0;
    }
    uint64_t batch = nextBatch_++;
    batches_[batch] = std::move(retired_);
    retired_.clear();
    return batch;
}

void TextureRegistry::Complete(uint64_t batch)
{
    std::vector<Shown> frames;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = batches_.find(batch);
        if (it == batches_.end())
        {
            return;
        }
        frames = std::move(it->second);
        batches_.erase(it);
    }
    for (Shown &f : frames)
    {
        // the import first: the picture is released before its frame can be written again
        f.imported.reset();
        if (f.handler)
        {
            releaseFrame_(f.handler, f.index);
        }
    }
}

void TextureRegistry::ForgetBuffer(MAF::BufferHandler *handler)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (Shown &f : retired_)
    {
        if (f.handler == handler)
        {
            f.handler = nullptr;
        }
    }
    for (auto &[batch, frames] : batches_)
    {
        for (Shown &f : frames)
        {
            if (f.handler == handler)
            {
                f.handler = nullptr;
            }
        }
    }
    for (auto &[key, f] : shown_)
    {
        if (f.handler == handler)
        {
            f.handler = nullptr;
        }
    }
}

size_t TextureRegistry::ShownFrames()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return shown_.size();
}

size_t TextureRegistry::PendingFrames()
{
    std::lock_guard<std::mutex> lock(mutex_);
    size_t n = retired_.size();
    for (auto &[batch, frames] : batches_)
    {
        n += frames.size();
    }
    return n;
}
