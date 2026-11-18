### the core Media Application Function library

Located in *subprojects/maf*, this is the core library for the MAF API exposed by media pipelines.

It includes a simple factory to register and retrieve media pipelines as plugins.

If your contribution makes changes to the public API (factory, plugins, MAF api), make sure to re-generate the SWIG wrappers.

#### Buffer API

`BufferHandler` implements the circular buffer of ISO/IEC 23090-14, clause 6.3.

`releaseFrame(index)` (Table 23) tells the buffer that the frame at `index` may be overwritten by the write process. A buffer can hold the frames it returns from `readFrame()` until they are released: a held frame is not overwritten, and `writeFrame()` / `writeFrame2()` throw when the frame at the write index is held. Writers check `isWritable(idx)` before writing a frame in place. Frames of the buffers allocated with `allocate(capacity, maxFrameSize)` are never held, and `releaseFrame()` has no effect on them.
