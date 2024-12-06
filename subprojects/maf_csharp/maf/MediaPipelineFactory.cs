//------------------------------------------------------------------------------
// <auto-generated />
//
// This file was automatically generated by SWIG (https://www.swig.org).
// Version 4.2.1
//
// Do not make changes to this file unless you know what you are doing - modify
// the SWIG interface file instead.
//------------------------------------------------------------------------------

namespace maf {

public class MediaPipelineFactory : global::System.IDisposable {
  private global::System.Runtime.InteropServices.HandleRef swigCPtr;
  protected bool swigCMemOwn;

  internal MediaPipelineFactory(global::System.IntPtr cPtr, bool cMemoryOwn) {
    swigCMemOwn = cMemoryOwn;
    swigCPtr = new global::System.Runtime.InteropServices.HandleRef(this, cPtr);
  }

  internal static global::System.Runtime.InteropServices.HandleRef getCPtr(MediaPipelineFactory obj) {
    return (obj == null) ? new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero) : obj.swigCPtr;
  }

  internal static global::System.Runtime.InteropServices.HandleRef swigRelease(MediaPipelineFactory obj) {
    if (obj != null) {
      if (!obj.swigCMemOwn)
        throw new global::System.ApplicationException("Cannot release ownership as memory is not owned");
      global::System.Runtime.InteropServices.HandleRef ptr = obj.swigCPtr;
      obj.swigCMemOwn = false;
      obj.Dispose();
      return ptr;
    } else {
      return new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
    }
  }

  ~MediaPipelineFactory() {
    Dispose(false);
  }

  public void Dispose() {
    Dispose(true);
    global::System.GC.SuppressFinalize(this);
  }

  protected virtual void Dispose(bool disposing) {
    lock(this) {
      if (swigCPtr.Handle != global::System.IntPtr.Zero) {
        if (swigCMemOwn) {
          swigCMemOwn = false;
          throw new global::System.MethodAccessException("C++ destructor does not have public access");
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
    }
  }

  public static void registerPlugin(string name, SWIGTYPE_p_std__functionT_MAF__IMediaPipeline_pfvoidF_t factoryFn) {
    mafPINVOKE.MediaPipelineFactory_registerPlugin(name, SWIGTYPE_p_std__functionT_MAF__IMediaPipeline_pfvoidF_t.getCPtr(factoryFn));
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
  }

  public static IMediaPipeline createMediaPipeline(MediaInfo mediaInfo, BufferInfoArray buffers) {
    global::System.IntPtr cPtr = mafPINVOKE.MediaPipelineFactory_createMediaPipeline(MediaInfo.getCPtr(mediaInfo), BufferInfoArray.getCPtr(buffers));
    IMediaPipeline ret = (cPtr == global::System.IntPtr.Zero) ? null : new IMediaPipeline(cPtr, false);
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
    return ret;
  }

}

}
