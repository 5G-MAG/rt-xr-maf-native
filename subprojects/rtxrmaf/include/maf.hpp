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

#if MAF_API_BUILD
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
public:
    BufferHandler()=default;
    ~BufferHandler()=default;
    std::vector<Frame>* getFrames();
    void allocate(uint8_t capacity);
    void allocate(uint8_t capacity, uint32_t maxFrameSize);
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
    // FIXME: workaround the current limitation of the API to signal the expected color format
    // when this is passed through, the other members
    TEXTURE_RGB,
    TEXTURE_GBR_3PLANE_420_UNORM
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

struct BufferInfo
{
    int bufferId;
    BufferHandler *handler;
    ComponentType componentType;
    SampleType type;
    int offset;
    int stride;
    AttributeType attributeType;
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

#ifdef SWIG_BINDINGS
class BufferInfoHeader
#else
class MAF_API BufferInfoHeader
#endif
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