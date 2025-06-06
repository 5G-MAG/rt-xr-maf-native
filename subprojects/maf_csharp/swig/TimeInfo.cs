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

public class TimeInfo : global::System.IDisposable {
  private global::System.Runtime.InteropServices.HandleRef swigCPtr;
  protected bool swigCMemOwn;

  internal TimeInfo(global::System.IntPtr cPtr, bool cMemoryOwn) {
    swigCMemOwn = cMemoryOwn;
    swigCPtr = new global::System.Runtime.InteropServices.HandleRef(this, cPtr);
  }

  internal static global::System.Runtime.InteropServices.HandleRef getCPtr(TimeInfo obj) {
    return (obj == null) ? new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero) : obj.swigCPtr;
  }

  internal static global::System.Runtime.InteropServices.HandleRef swigRelease(TimeInfo obj) {
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

  ~TimeInfo() {
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
          mafPINVOKE.delete_TimeInfo(swigCPtr);
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
    }
  }

  public double startTime {
    set {
      mafPINVOKE.TimeInfo_startTime_set(swigCPtr, value);
    } 
    get {
      double ret = mafPINVOKE.TimeInfo_startTime_get(swigCPtr);
      return ret;
    } 
  }

  public double timeOffset {
    set {
      mafPINVOKE.TimeInfo_timeOffset_set(swigCPtr, value);
    } 
    get {
      double ret = mafPINVOKE.TimeInfo_timeOffset_get(swigCPtr);
      return ret;
    } 
  }

  public bool autoplay {
    set {
      mafPINVOKE.TimeInfo_autoplay_set(swigCPtr, value);
    } 
    get {
      bool ret = mafPINVOKE.TimeInfo_autoplay_get(swigCPtr);
      return ret;
    } 
  }

  public bool loop {
    set {
      mafPINVOKE.TimeInfo_loop_set(swigCPtr, value);
    } 
    get {
      bool ret = mafPINVOKE.TimeInfo_loop_get(swigCPtr);
      return ret;
    } 
  }

  public TimeInfo() : this(mafPINVOKE.new_TimeInfo(), true) {
  }

}

}
