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

#pragma once

#include <memory>

extern "C"
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef _MSC_VER
#pragma warning(default : 4244)
#endif
}

#include <maf.hpp>

// A video decoder of a stream: the libav codec context VideoDecoder feeds and reads, and the operations that depend on
// where the decoded pictures are (system memory, or the memory of a hardware decoder).
class VideoDecodeBackend
{
public:
    virtual ~VideoDecodeBackend() = default;
    // name of the backend, for logs
    virtual const char *Name() const = 0;
    virtual AVCodecContext *Context() = 0;
    // The decoded picture in system memory: decoded itself when it already is, otherwise sw holds a copy of it.
    // Returns nullptr when the picture can not be copied.
    virtual AVFrame *ToSoftware(AVFrame *decoded, AVFrame *sw)
    {
        (void)sw;
        return decoded;
    }
};

// Opens the decoder of a video stream with the first backend of the platform that can decode it, the software
// decoder last. Throws when no backend can decode the stream.
std::unique_ptr<VideoDecodeBackend> CreateVideoBackend(AVStream *stream, const MAF::BufferInfo &info);

// Opens the libav software decoder of a stream. Throws a libav error code on failure.
AVCodecContext *OpenSoftwareDecoder(AVStream *stream);
