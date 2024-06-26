//------------------------------------------------------------------------------
// <auto-generated />
//
// This file was automatically generated by SWIG (https://www.swig.org).
// Version 4.1.1
//
// Do not make changes to this file unless you know what you are doing - modify
// the SWIG interface file instead.
//------------------------------------------------------------------------------

namespace maf {

public class PluginArray : global::System.IDisposable, global::System.Collections.IEnumerable, global::System.Collections.Generic.IList<IMediaPipelineFactoryPlugin>
 {
  private global::System.Runtime.InteropServices.HandleRef swigCPtr;
  protected bool swigCMemOwn;

  internal PluginArray(global::System.IntPtr cPtr, bool cMemoryOwn) {
    swigCMemOwn = cMemoryOwn;
    swigCPtr = new global::System.Runtime.InteropServices.HandleRef(this, cPtr);
  }

  internal static global::System.Runtime.InteropServices.HandleRef getCPtr(PluginArray obj) {
    return (obj == null) ? new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero) : obj.swigCPtr;
  }

  internal static global::System.Runtime.InteropServices.HandleRef swigRelease(PluginArray obj) {
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

  ~PluginArray() {
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
          mafPINVOKE.delete_PluginArray(swigCPtr);
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
    }
  }

  public PluginArray(global::System.Collections.IEnumerable c) : this() {
    if (c == null)
      throw new global::System.ArgumentNullException("c");
    foreach (IMediaPipelineFactoryPlugin element in c) {
      this.Add(element);
    }
  }

  public PluginArray(global::System.Collections.Generic.IEnumerable<IMediaPipelineFactoryPlugin> c) : this() {
    if (c == null)
      throw new global::System.ArgumentNullException("c");
    foreach (IMediaPipelineFactoryPlugin element in c) {
      this.Add(element);
    }
  }

  public bool IsFixedSize {
    get {
      return false;
    }
  }

  public bool IsReadOnly {
    get {
      return false;
    }
  }

  public IMediaPipelineFactoryPlugin this[int index]  {
    get {
      return getitem(index);
    }
    set {
      setitem(index, value);
    }
  }

  public int Capacity {
    get {
      return (int)capacity();
    }
    set {
      if (value < 0 || (uint)value < size())
        throw new global::System.ArgumentOutOfRangeException("Capacity");
      reserve((uint)value);
    }
  }

  public int Count {
    get {
      return (int)size();
    }
  }

  public bool IsSynchronized {
    get {
      return false;
    }
  }

  public void CopyTo(IMediaPipelineFactoryPlugin[] array)
  {
    CopyTo(0, array, 0, this.Count);
  }

  public void CopyTo(IMediaPipelineFactoryPlugin[] array, int arrayIndex)
  {
    CopyTo(0, array, arrayIndex, this.Count);
  }

  public void CopyTo(int index, IMediaPipelineFactoryPlugin[] array, int arrayIndex, int count)
  {
    if (array == null)
      throw new global::System.ArgumentNullException("array");
    if (index < 0)
      throw new global::System.ArgumentOutOfRangeException("index", "Value is less than zero");
    if (arrayIndex < 0)
      throw new global::System.ArgumentOutOfRangeException("arrayIndex", "Value is less than zero");
    if (count < 0)
      throw new global::System.ArgumentOutOfRangeException("count", "Value is less than zero");
    if (array.Rank > 1)
      throw new global::System.ArgumentException("Multi dimensional array.", "array");
    if (index+count > this.Count || arrayIndex+count > array.Length)
      throw new global::System.ArgumentException("Number of elements to copy is too large.");
    for (int i=0; i<count; i++)
      array.SetValue(getitemcopy(index+i), arrayIndex+i);
  }

  public IMediaPipelineFactoryPlugin[] ToArray() {
    IMediaPipelineFactoryPlugin[] array = new IMediaPipelineFactoryPlugin[this.Count];
    this.CopyTo(array);
    return array;
  }

  global::System.Collections.Generic.IEnumerator<IMediaPipelineFactoryPlugin> global::System.Collections.Generic.IEnumerable<IMediaPipelineFactoryPlugin>.GetEnumerator() {
    return new PluginArrayEnumerator(this);
  }

  global::System.Collections.IEnumerator global::System.Collections.IEnumerable.GetEnumerator() {
    return new PluginArrayEnumerator(this);
  }

  public PluginArrayEnumerator GetEnumerator() {
    return new PluginArrayEnumerator(this);
  }

  // Type-safe enumerator
  /// Note that the IEnumerator documentation requires an InvalidOperationException to be thrown
  /// whenever the collection is modified. This has been done for changes in the size of the
  /// collection but not when one of the elements of the collection is modified as it is a bit
  /// tricky to detect unmanaged code that modifies the collection under our feet.
  public sealed class PluginArrayEnumerator : global::System.Collections.IEnumerator
    , global::System.Collections.Generic.IEnumerator<IMediaPipelineFactoryPlugin>
  {
    private PluginArray collectionRef;
    private int currentIndex;
    private object currentObject;
    private int currentSize;

    public PluginArrayEnumerator(PluginArray collection) {
      collectionRef = collection;
      currentIndex = -1;
      currentObject = null;
      currentSize = collectionRef.Count;
    }

    // Type-safe iterator Current
    public IMediaPipelineFactoryPlugin Current {
      get {
        if (currentIndex == -1)
          throw new global::System.InvalidOperationException("Enumeration not started.");
        if (currentIndex > currentSize - 1)
          throw new global::System.InvalidOperationException("Enumeration finished.");
        if (currentObject == null)
          throw new global::System.InvalidOperationException("Collection modified.");
        return (IMediaPipelineFactoryPlugin)currentObject;
      }
    }

    // Type-unsafe IEnumerator.Current
    object global::System.Collections.IEnumerator.Current {
      get {
        return Current;
      }
    }

    public bool MoveNext() {
      int size = collectionRef.Count;
      bool moveOkay = (currentIndex+1 < size) && (size == currentSize);
      if (moveOkay) {
        currentIndex++;
        currentObject = collectionRef[currentIndex];
      } else {
        currentObject = null;
      }
      return moveOkay;
    }

    public void Reset() {
      currentIndex = -1;
      currentObject = null;
      if (collectionRef.Count != currentSize) {
        throw new global::System.InvalidOperationException("Collection modified.");
      }
    }

    public void Dispose() {
        currentIndex = -1;
        currentObject = null;
    }
  }

  public void Clear() {
    mafPINVOKE.PluginArray_Clear(swigCPtr);
  }

  public void Add(IMediaPipelineFactoryPlugin x) {
    mafPINVOKE.PluginArray_Add(swigCPtr, IMediaPipelineFactoryPlugin.getCPtr(x));
  }

  private uint size() {
    uint ret = mafPINVOKE.PluginArray_size(swigCPtr);
    return ret;
  }

  private uint capacity() {
    uint ret = mafPINVOKE.PluginArray_capacity(swigCPtr);
    return ret;
  }

  private void reserve(uint n) {
    mafPINVOKE.PluginArray_reserve(swigCPtr, n);
  }

  public PluginArray() : this(mafPINVOKE.new_PluginArray__SWIG_0(), true) {
  }

  public PluginArray(PluginArray other) : this(mafPINVOKE.new_PluginArray__SWIG_1(PluginArray.getCPtr(other)), true) {
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
  }

  public PluginArray(int capacity) : this(mafPINVOKE.new_PluginArray__SWIG_2(capacity), true) {
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
  }

  private IMediaPipelineFactoryPlugin getitemcopy(int index) {
    global::System.IntPtr cPtr = mafPINVOKE.PluginArray_getitemcopy(swigCPtr, index);
    IMediaPipelineFactoryPlugin ret = (cPtr == global::System.IntPtr.Zero) ? null : new IMediaPipelineFactoryPlugin(cPtr, false);
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
    return ret;
  }

  private IMediaPipelineFactoryPlugin getitem(int index) {
    global::System.IntPtr cPtr = mafPINVOKE.PluginArray_getitem(swigCPtr, index);
    IMediaPipelineFactoryPlugin ret = (cPtr == global::System.IntPtr.Zero) ? null : new IMediaPipelineFactoryPlugin(cPtr, false);
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
    return ret;
  }

  private void setitem(int index, IMediaPipelineFactoryPlugin val) {
    mafPINVOKE.PluginArray_setitem(swigCPtr, index, IMediaPipelineFactoryPlugin.getCPtr(val));
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
  }

  public void AddRange(PluginArray values) {
    mafPINVOKE.PluginArray_AddRange(swigCPtr, PluginArray.getCPtr(values));
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
  }

  public PluginArray GetRange(int index, int count) {
    global::System.IntPtr cPtr = mafPINVOKE.PluginArray_GetRange(swigCPtr, index, count);
    PluginArray ret = (cPtr == global::System.IntPtr.Zero) ? null : new PluginArray(cPtr, true);
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
    return ret;
  }

  public void Insert(int index, IMediaPipelineFactoryPlugin x) {
    mafPINVOKE.PluginArray_Insert(swigCPtr, index, IMediaPipelineFactoryPlugin.getCPtr(x));
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
  }

  public void InsertRange(int index, PluginArray values) {
    mafPINVOKE.PluginArray_InsertRange(swigCPtr, index, PluginArray.getCPtr(values));
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
  }

  public void RemoveAt(int index) {
    mafPINVOKE.PluginArray_RemoveAt(swigCPtr, index);
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
  }

  public void RemoveRange(int index, int count) {
    mafPINVOKE.PluginArray_RemoveRange(swigCPtr, index, count);
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
  }

  public static PluginArray Repeat(IMediaPipelineFactoryPlugin value, int count) {
    global::System.IntPtr cPtr = mafPINVOKE.PluginArray_Repeat(IMediaPipelineFactoryPlugin.getCPtr(value), count);
    PluginArray ret = (cPtr == global::System.IntPtr.Zero) ? null : new PluginArray(cPtr, true);
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
    return ret;
  }

  public void Reverse() {
    mafPINVOKE.PluginArray_Reverse__SWIG_0(swigCPtr);
  }

  public void Reverse(int index, int count) {
    mafPINVOKE.PluginArray_Reverse__SWIG_1(swigCPtr, index, count);
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
  }

  public void SetRange(int index, PluginArray values) {
    mafPINVOKE.PluginArray_SetRange(swigCPtr, index, PluginArray.getCPtr(values));
    if (mafPINVOKE.SWIGPendingException.Pending) throw mafPINVOKE.SWIGPendingException.Retrieve();
  }

  public bool Contains(IMediaPipelineFactoryPlugin value) {
    bool ret = mafPINVOKE.PluginArray_Contains(swigCPtr, IMediaPipelineFactoryPlugin.getCPtr(value));
    return ret;
  }

  public int IndexOf(IMediaPipelineFactoryPlugin value) {
    int ret = mafPINVOKE.PluginArray_IndexOf(swigCPtr, IMediaPipelineFactoryPlugin.getCPtr(value));
    return ret;
  }

  public int LastIndexOf(IMediaPipelineFactoryPlugin value) {
    int ret = mafPINVOKE.PluginArray_LastIndexOf(swigCPtr, IMediaPipelineFactoryPlugin.getCPtr(value));
    return ret;
  }

  public bool Remove(IMediaPipelineFactoryPlugin value) {
    bool ret = mafPINVOKE.PluginArray_Remove(swigCPtr, IMediaPipelineFactoryPlugin.getCPtr(value));
    return ret;
  }

}

}
