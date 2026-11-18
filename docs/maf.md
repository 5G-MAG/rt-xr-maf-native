### the core Media Application Function library

Located in *subprojects/maf*, this is the core library for the MAF API exposed by media pipelines.

It includes a simple factory to register and retrieve media pipelines as plugins.

If your contribution makes changes to the public API (factory, plugins, MAF api), make sure to re-generate the SWIG wrappers.

#### Buffer API

`BufferHandler` implements the circular buffer of ISO/IEC 23090-14, clause 6.3.

`releaseFrame(index)` (Table 23) tells the buffer that the frame at `index` may be overwritten by the write process. A buffer can hold the frames it returns from `readFrame()` until they are released: a held frame is not overwritten, and `writeFrame()` / `writeFrame2()` throw when the frame at the write index is held. Writers check `isWritable(idx)` before writing a frame in place. Frames of the buffers allocated with `allocate(capacity, maxFrameSize)` are never held, and `releaseFrame()` has no effect on them.

#### Native frame buffers

*Implementation convention, not part of ISO/IEC 23090-14.*

A texture buffer can store its pictures in memory that the GPU imports without a copy, rather than as bytes in `Frame.data`. The Presentation Engine requests it for a buffer with `BufferInfo.outputMedia.memory = FRAME_MEMORY_NATIVE`, and allocates the buffer with `BufferHandler::allocateNative(capacity)`.

In a native buffer:
- `Frame.data` is a handle to the picture, whose type depends on the platform the library is built for:

  | Platform | `Frame.data` |
  |---|---|
  | Apple | `CVPixelBufferRef` |
  | Android | `AHardwareBuffer*` |

- `Frame.length` is 0. The width and height of the picture are in the frame (`extraFrameInfoFlags` bit 0, Table 25).
- The handle is owned by the writer. A frame returned by `readFrame()` is held until the consumer calls `releaseFrame(index)`, for example once the GPU is done with the picture. The consumer keeps its own reference to the handle if it uses it after that.
- `free()` does not release the handles.
- The picture holds the samples of `outputMedia.format`, in a layout of the platform that the handle describes (its pixel format and planes). A format the platform has no layout for is stored in an equivalent one with the same samples:

  | Requested format | Apple (`CVPixelBuffer` pixel format) |
  |---|---|
  | `RGB`, `RGBA`, `BGR`, `BGRA` | `32BGRA` (alpha 1 when the format has none) |
  | `RED`, `GREEN`, `BLUE`, `DEPTH_COMPONENT` / `RG` | `OneComponent8` / `TwoComponent8` |
  | `G8_B8_R8_3PLANE_420_UNORM` | `420YpCbCr8Planar` |
  | 2-plane, and 3-plane 4:2:2 and 4:4:4, YCbCr formats | the 2-plane `YpCbCr…BiPlanar` format with the same sampling and bit depth |

  YCbCr pictures use the full range variant of the pixel format when the decoded picture does.

The Presentation Engine checks `initialize()`: the pipeline is in the `ERROR` state when a native buffer is not allocated with `allocateNative()`, has more than one accessor, has no `outputMedia` size, is not mapped to a video track, or requests a format the platform can not store natively.

Native buffers are built in with the `native_frames` option of `maf` (`auto`, the default, enables them on Apple and Android; `none` disables them):
```
meson setup -Dmaf:native_frames=none builddir
```
When they are built in, `maf_config.hpp` defines `MAF_NATIVE_FRAMES`. Otherwise `allocateNative()` throws.
