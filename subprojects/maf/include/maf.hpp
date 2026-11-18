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


#ifndef _MEDIA_ACCESS_FUNCTIONS_
#define _MEDIA_ACCESS_FUNCTIONS_

#include<string>
#include<vector>
#include<iostream>
#include<mutex>
#include<condition_variable>

#include "maf_config.hpp"

#if defined(_MSC_VER)
    #define EXPORT __declspec(dllexport)
    #define IMPORT __declspec(dllimport)
#elif defined(__GNUC__)
    #define EXPORT __attribute__((visibility("default")))
    #define IMPORT
#else
    #define EXPORT
    #define IMPORT
    #pragma warning Unknown dynamic link import/export semantics.
#endif

#if defined(MAF_API_EXPORT)
    #define MAF_API EXPORT
#else
    #define MAF_API IMPORT
#endif

namespace MAF {

/* View ***********************************************************************/
    
typedef float Position[3]; 
typedef float Quaternion[4];
typedef float Transform[4][4];

struct Pose {
    Position position;
    Quaternion orientation;
};

enum CameraProjectionType {
    PERSPECTIVE,
    ORTHOGRAPHIC
};

struct PerspectiveCameraViewingVolume { 
    double aspectRatio; 
    double yFov; 
}; 

struct OrthographicCameraViewingVolume { 
    double xmag; 
    double ymag; 
};

struct Camera {
    const CameraProjectionType* type;
    const PerspectiveCameraViewingVolume* perspectiveCamViewVolume; 
    const OrthographicCameraViewingVolume* orthographicCamViewVolume;
    const double* zNear; 
    const double* zFar; 
};

struct ViewInfo
{
    Pose pose;
    Camera camera;
    Transform objectTransform;
};



/* Buffer ***********************************************************************/

class Frame
{
public:
    uint8_t index;
    uint64_t timestamp;
    uint32_t length;
    uint8_t * data;
    // Table 25: bit 0 of extraFrameInfoFlags (the dimensions flag) is set when the frame holds
    // texture data; width and height are then the size of the picture stored in the frame.
    uint8_t extraFrameInfoFlags = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    Frame(uint8_t i = 0, uint64_t t = 0, uint32_t byteLength = 0, uint8_t* buffer = nullptr);
    ~Frame() = default;
};

typedef void (*Callback)(); 

class BufferHandler
{
private:
    std::condition_variable cvReadFrame_;
    std::mutex mutex_; 
    std::vector<Frame> frames_;
    uint8_t capacity_ {0};
    uint8_t count_{0};
    uint8_t readIdx_ {0};
    uint8_t writeIdx_ {0};
    Callback onFrameWrite_ = nullptr;
    Callback onFrameRead_ = nullptr;
    // void (*onFrameWrite_)();
    // void (*onFrameRead_)();
    int presentationFrameIdx_ {-1};
    // ISO/IEC 23090-14, clause 6.3: a held frame is not overwritten by the write process until releaseFrame() is
    // called with its index. Frames are held when read only if holdOnRead_ is set.
    std::vector<bool> held_;
    bool holdOnRead_ {false};
    bool native_ {false};
    void hold(int idx); // requires mutex_
public:
    BufferHandler()=default;
    ~BufferHandler()=default;
    std::vector<Frame>* getFrames();
    void allocate(uint8_t capacity);
    void allocate(uint8_t capacity, uint32_t maxFrameSize);
    // IMPLEMENTATION CONVENTION, NOT PART OF ISO/IEC 23090-14: allocates a native frame buffer, for a texture whose
    // BufferInfo.outputMedia.memory is FRAME_MEMORY_NATIVE. Frame.data of each frame written to it is a handle to the
    // picture, owned by the writer: a CVPixelBufferRef on Apple platforms, an AHardwareBuffer* on Android; Frame.length
    // is 0. Each frame read is held until releaseFrame() is called with its index. free() does not release the handles.
    // Throws when the library is built without native frame buffers (MAF_NATIVE_FRAMES is not defined).
    void allocateNative(uint8_t capacity);
    uint8_t capacity();
    uint8_t count();
    void free();
    int getReadIdx();
    int getWriteIdx();
    int headerLength;
    void writeFrame(Frame* frame); // blocks
    void writeFrame2(Frame* frame); // full
    void writeFrame3(Frame* frame); // realtime
    Frame* readFrame();
    Frame* readFrame(uint64_t *timestamp); // timestamp formated as ntp without epoch
    Frame* readFrame(double time_s); // see: Table 25 - IDL of buffer API 
    // Frame* readFrame(uint64_t time_us); 
    // ISO/IEC 23090-14, clause 6.3 (Table 23): the data of the frame at index may be overwritten by the write process
    void releaseFrame(uint8_t index);
    // whether the write process may write the frame at idx: the buffer is not full there and the frame is not held
    bool isWritable(int idx);
    void setOnFrameWriteCallback(Callback onFrameWrite);
    void setOnFrameReadCallback(Callback onFrameRead);
    // void setOnFrameWriteCallback(void (*onFrameWrite)());
    // void setOnFrameReadCallback(void (*onFrameRead)());
};



/* BufferInfo ***********************************************************************/

// defined as strings by gltf
enum SampleType {
    SCALAR = 0, 
    VEC2, 
    VEC3, 
    VEC4, 
    MAT2, 
    MAT3, 
    MAT4
};

// 
enum AttributeType {
    ATTRIB_NORMAL,
    ATTRIB_POSITION,
    ATTRIB_COLOR, 
    ATTRIB_TEXCOORD,
    ATTRIB_INDEX,
    ATTRIB_TANGENT, 
    ATTRIB_WEIGHTS, 
    ATTRIB_JOINTS,
    // ISO/IEC 23090-14, clause 6.2: PCM audio data, and binary metadata whose format depends on the referencing
    // component or extension. The format of a texture is not an attribute: it is BufferInfo.outputMedia.format.
    ATTRIB_PCM,
    ATTRIB_METADATA
};


// as defined by gltf
enum ComponentType 
{
    BYTE			= 5120,
    UNSIGNED_BYTE	= 5121,
    SHORT			= 5122,
    UNSIGNED_SHORT	= 5123,
    UNSIGNED_INT	= 5125,
    FLOAT			= 5126
};

// ISO/IEC 23090-14, clause 6.2: the format of the pixel data of a texture
enum ColorFormat {
    RED,
    GREEN,
    BLUE,
    RG,
    RGB,
    RGBA,
    BGR,
    BGRA,
    DEPTH_COMPONENT,
    // MPEG_texture_video YCbCr formats (Vulkan 1.3, Table 76), not in the IDL of clause 6.2. The planes are stored one
    // after the other without padding. 3PLANE: the Y plane, then the Cb plane, then the Cr plane. 2PLANE: the Y plane,
    // then one plane of interleaved Cb and Cr samples. The chroma planes are at half the width and height of the picture
    // for 420, half the width for 422, and full size for 444 (halves rounded up).
    // 8-bit formats have one byte per sample (accessor componentType UNSIGNED_BYTE). 10-bit formats (G10X6) have one
    // little-endian 16-bit word per sample holding the value in its 10 most significant bits (UNSIGNED_SHORT).
    G8_B8_R8_3PLANE_420_UNORM,
    G8_B8R8_2PLANE_420_UNORM,
    G8_B8_R8_3PLANE_422_UNORM,
    G8_B8R8_2PLANE_422_UNORM,
    G8_B8_R8_3PLANE_444_UNORM,
    G8_B8R8_2PLANE_444_UNORM,
    G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
    G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
    G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
    G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
    G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
    G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16
};

// IMPLEMENTATION CONVENTION, NOT PART OF ISO/IEC 23090-14: where the frames of a buffer store the picture
enum FrameMemory {
    // in Frame.data, as bytes (clause 6.3)
    FRAME_MEMORY_CPU = 0,
    // Frame.data is a handle to the picture in memory the GPU can import, see BufferHandler::allocateNative()
    FRAME_MEMORY_NATIVE = 1
};

// ISO/IEC 23090-14, clause 6.2: the format of the media the pipeline outputs. width and height are those of a texture
// (the maximum picture size of MPEG_texture_video), format its pixel format, sampleRate the sample rate of audio.
// A width, height or sampleRate of 0 is not set: the decoded value is kept.
struct OutputMedia
{
    uint32_t width = 0;
    uint32_t height = 0;
    ColorFormat format = ColorFormat::RGB;
    uint32_t sampleRate = 0;
    // IMPLEMENTATION CONVENTION, NOT PART OF ISO/IEC 23090-14: the memory the frames of a texture are stored in.
    // FRAME_MEMORY_NATIVE requires a buffer allocated with BufferHandler::allocateNative().
    FrameMemory memory = FRAME_MEMORY_CPU;
};

struct BufferInfo
{
    int bufferId;
    BufferHandler *handler;
    ComponentType componentType;
    SampleType type;
    int offset;
    int stride;
    AttributeType attributeType;
    OutputMedia outputMedia;
};



/* Pipeline ***********************************************************************/


struct Track
{
    std::string track;
    int id;
    int bufferId;
};

struct AlternativeLocation
{
    std::string mimeType;
    std::vector<Track> tracks;
    std::string uri;
};

struct MediaInfo
{
    std::string name;
    std::vector<AlternativeLocation> alternatives;
};

struct TimeInfo
{
    double startTime;
    double timeOffset;
    bool autoplay;
    bool loop;
};

enum PipelineState {
    IDLE = 0,
    READY = 1,
    ACTIVE = 2,
    ERROR = 3
};

class IMediaPipeline
{ 
public:
    virtual ~IMediaPipeline()=default;
    virtual BufferInfo* bufferInfo(int idx)=0;
    virtual PipelineState state()=0;
    virtual void initialize(MediaInfo mediaInfo, std::vector<BufferInfo> buffers)=0;
    virtual void setStateChangeCallback(void (*onStateChange)())=0;
    virtual void startFetching(TimeInfo timeInfo, ViewInfo viewInfo)=0;
    virtual void updateView(ViewInfo viewInfo)=0;
    virtual void stopFetching()=0;
};

typedef std::pair<BufferHandler*, std::vector<BufferInfo>> MafBuffer;

extern int componentSize(ComponentType ct);
extern int sampleSize(SampleType st);

// #ifdef SWIG_BINDINGS
// class BufferInfoHeader
// #else
// class MAF_API BufferInfoHeader
// #endif
class MAF_API BufferInfoHeader
{
    public:
        float timestampDelta = 0.0;
        bool immutable = true;
        ComponentType componentType = ComponentType::UNSIGNED_BYTE;
        SampleType type = SampleType::SCALAR;
        bool normalized = false;
        uint32_t byteOffset = 0;
        uint32_t count = 0;
        void* max = nullptr;
        void* min = nullptr;
        uint32_t bufferViewByteOffset = 0;
        uint32_t bufferViewByteLength = 0;
        uint32_t bufferViewByteStride = 0;
        int GetHeaderSize();
        void WriteAccessorHeader(uint8_t* data);
        void ReadAccessorHeader(uint8_t* data);
        static std::vector<BufferInfoHeader>* CreateAccessorsHeaders(std::vector<BufferInfo>* bufferInfo, bool immutable);
        static void ComputeAccessorsHeadersInterleaved(std::vector<BufferInfoHeader>* headers);
        static void ComputeAccessorsHeadersPlanar(std::vector<BufferInfoHeader>* headers);
        static void ComputeTotalHeadersLength(std::vector<BufferInfoHeader>* headers, int &bytelength);          
        static void WriteAccessorsHeaders(uint8_t* data, std::vector<BufferInfoHeader>* headers);
        static void ReadAccessorsHeaders(uint8_t* data, std::vector<BufferInfoHeader>* headers);
};



/* Timestamp ***********************************************************************/

extern uint64_t getTimestamp(double time_s);

extern uint64_t getTimestamp(int64_t time_us);

extern double getTime_s(uint64_t timestamp);

/*
static uint64_t getTime_us(uint64_t timestamp){
    uint64_t integral = (timestamp >> 32) * 1000000;
    uint64_t fractional = (timestamp & 0xFFFFFFFFull);
    return integral + ((fractional * 1000000) / 0xFFFFFFFFull);
};
*/

} // namespace MAF

#endif // _MEDIA_ACCESS_FUNCTIONS_