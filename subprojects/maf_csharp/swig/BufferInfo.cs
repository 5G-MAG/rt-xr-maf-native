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

public class BufferInfo : global::System.IDisposable {
  private global::System.Runtime.InteropServices.HandleRef swigCPtr;
  protected bool swigCMemOwn;

  internal BufferInfo(global::System.IntPtr cPtr, bool cMemoryOwn) {
    swigCMemOwn = cMemoryOwn;
    swigCPtr = new global::System.Runtime.InteropServices.HandleRef(this, cPtr);
  }

  internal static global::System.Runtime.InteropServices.HandleRef getCPtr(BufferInfo obj) {
    return (obj == null) ? new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero) : obj.swigCPtr;
  }

  internal static global::System.Runtime.InteropServices.HandleRef swigRelease(BufferInfo obj) {
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

  ~BufferInfo() {
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
          mafPINVOKE.delete_BufferInfo(swigCPtr);
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
    }
  }

  public int bufferId {
    set {
      mafPINVOKE.BufferInfo_bufferId_set(swigCPtr, value);
    } 
    get {
      int ret = mafPINVOKE.BufferInfo_bufferId_get(swigCPtr);
      return ret;
    } 
  }

  public BufferHandler handler {
    set {
      mafPINVOKE.BufferInfo_handler_set(swigCPtr, BufferHandler.getCPtr(value));
    } 
    get {
      global::System.IntPtr cPtr = mafPINVOKE.BufferInfo_handler_get(swigCPtr);
      BufferHandler ret = (cPtr == global::System.IntPtr.Zero) ? null : new BufferHandler(cPtr, false);
      return ret;
    } 
  }

  public ComponentType componentType {
    set {
      mafPINVOKE.BufferInfo_componentType_set(swigCPtr, (int)value);
    } 
    get {
      ComponentType ret = (ComponentType)mafPINVOKE.BufferInfo_componentType_get(swigCPtr);
      return ret;
    } 
  }

  public SampleType type {
    set {
      mafPINVOKE.BufferInfo_type_set(swigCPtr, (int)value);
    } 
    get {
      SampleType ret = (SampleType)mafPINVOKE.BufferInfo_type_get(swigCPtr);
      return ret;
    } 
  }

  public int offset {
    set {
      mafPINVOKE.BufferInfo_offset_set(swigCPtr, value);
    } 
    get {
      int ret = mafPINVOKE.BufferInfo_offset_get(swigCPtr);
      return ret;
    } 
  }

  public int stride {
    set {
      mafPINVOKE.BufferInfo_stride_set(swigCPtr, value);
    } 
    get {
      int ret = mafPINVOKE.BufferInfo_stride_get(swigCPtr);
      return ret;
    } 
  }

  public AttributeType attributeType {
    set {
      mafPINVOKE.BufferInfo_attributeType_set(swigCPtr, (int)value);
    } 
    get {
      AttributeType ret = (AttributeType)mafPINVOKE.BufferInfo_attributeType_get(swigCPtr);
      return ret;
    } 
  }

  public BufferInfo() : this(mafPINVOKE.new_BufferInfo(), true) {
  }

}

}
