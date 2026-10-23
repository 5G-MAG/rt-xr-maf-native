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

#include <Decoder.hpp>

// Decoder of the timed metadata tracks of ISO/IEC 23090-14:2024, clause 7. A metadata sample is not decoded: its bytes
// are published as one buffer frame whose timestamp is the presentation time of the sample.
//
//   'glat' (7.6)   animation timing; the sample is published unchanged in the data of a timed accessor (BYTE, SCALAR).
//   'mett' (7.2)   glTF JSON documents and JSON patch documents (scene updates); the buffer has no accessor
//                  (BufferInfo.attributeType METADATA_SCENE_UPDATE) and each frame holds
//                  [flags: 1 byte, bit 0 set for a sync sample][update_mode: 1 byte, SCENE_UPDATE_MODE_UNKNOWN][sample bytes].
//   'vcor' (7.4) and 'post' (7.5)  not published: their fields are fixed by the sample syntax in units whose byte order
//                  ISO/IEC 14496-12 defines, and that document was not available.
class MetadataDecoder : public Decoder
{
public:
    enum class Kind
    {
        SCENE_UPDATE,   // 'mett'
        ANIMATION,      // 'glat'
        CORRESPONDENCE, // 'vcor'
        POSE            // 'post'
    };

    static const size_t SCENE_UPDATE_PREFIX_BYTES = 2;
    static const uint8_t SCENE_UPDATE_FLAG_SYNC = 0x01;
    // update_mode is defined by the GLTFPatchConfigBox of the sample entry, which libavformat does not expose
    static const uint8_t SCENE_UPDATE_MODE_UNKNOWN = 0xFF;

    // codec_tag of a metadata stream as set by the ISOBMFF demuxer (the sample entry type)
    static bool KindFromTag(uint32_t codecTag, Kind &kind);

    MetadataDecoder(AVStream *stream, Kind kind);
    ~MetadataDecoder();
    void QueuePacket(AVPacket *pkt);
    void DecodeInto(MafBuffer *mafBuffer);
    void Start(MafBuffer *mafBuffer);
    void Stop();
    bool IsRunning();
    void SetRange(double startSource_s, double endSource_s, bool trimStart);
    void QueueLoop(double newBase_s);
    void QueueEnd();
    bool IsDone();
    bool HasError();
    size_t QueuedPackets();

private:
    AVStream *stream_;
    Kind kind_;
    std::thread thread_;
    std::exception_ptr error_;
    DecoderQueue queue_;
    std::atomic_bool quit_{false};
    std::atomic_bool done_{false};
    std::atomic_bool failed_{false};
    double rangeStart_ = 0.0;
    double rangeEnd_ = -1.0;
    bool trimStart_ = false;
    uint32_t maxFrameBytes_ = 0;
};

// Creates the decoder of a DATA stream, or throws when the sample entry type is not one of the four above.
Decoder *CreateMetadataDecoder(AVFormatContext *fmtCtx, uint32_t streamIdx);
