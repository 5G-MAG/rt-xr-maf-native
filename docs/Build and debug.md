# Build and debug 

1. build.sh

`meson setup install -Ddebug=true`

2. install.sh

When replacing the Media Pipeline libraries, make sur libraries are loaded.

/!\ Make sure that:
- libs are correctly configured in Unity
- all dependencies are added to the project (libav*, libc++, ...) 


3. Patch mafPInvoke.cs

```
// see this SWIG issue: https://github.com/swig/swig/pull/1262
// see also: https://discussions.unity.com/t/unity-ios-function-pointers/32466

internal class MonoPInvokeCallbackAttribute : System.Attribute
{
    public MonoPInvokeCallbackAttribute() { }
}
```

4. Build / Export Android Studio project:  https://docs.unity3d.com/Manual/android-export-process.html

eg. `Build/project/` - Do not rename/move the folder, as the project is generated with absolute paths.


5. Debug using the Android Studio project 

- configure debugger type to native only /!\
- build/debug the project
- just drag and drop C/C++ source in studio to set breakpoints

---

Troubleshooting ...

Check thatt libC++ is in the project















# DEAD END

3. Build & run XR Player for Android 

4. debuging C/C++ running on device

See: https://stackoverflow.com/questions/53733781/how-do-i-use-lldb-to-debug-c-code-on-android-on-command-line


```
UNITY_NDK_ROOT=/Applications/Unity/Hub/Editor/2022.3.34f1/PlaybackEngines/AndroidPlayer
alias adb=$UNITY_NDK_ROOT/SDK/platform-tools/adb

### run lldb-server on device
adb push $UNITY_NDK_ROOT/NDK/toolchains/llvm/prebuilt/darwin-x86_64/lib64/clang/12.0.8/lib/linux/aarch64/lldb-server /data/local/tmp
adb shell '/data/local/tmp/lldb-server platform --listen "*:10086" --server'

### forward port
adb forward tcp:10086 tcp:10086
adb forward --list

adb devices -l
...
lldb 
    platform select remote-android
    platform connect connect://39688bd9:10086
```