
To enable compilation of this plugin, use the `-Davpipeline=true` defined in [meson.options](../meson.options) and used in [meson.build](../meson.build) at the root of the repository for details.

All the commands below are meant to *run from the root of this repository*.


## Dependencies

This `avpipeline` plugin has a dependency on ffmpeg 7.1 libraries. These libraries are linked against as shared library.

They must be copied in a subdirectory of `./subprojects/avpipeline/external` in order to compile the plugin.

When setting up the project's compilation, that subdirectory's path must be passed explicitly with the `-Davpipeline:avcodec_dir` option. The path must be relative to `./subprojects/avpipeline`.


## Building

1. Copy the dependencies to a platform specific subdirectory, eg. in `./subprojects/avpipeline/external/avcodec/windows`.
2. Setup and compile `avpipeline` 
```
meson setup --wipe -Davpipeline=true -Davpipeline:avcodec_dir=external/avcodec/windows ./build/windows
meson compile -C ./build/windows
```


### Android

The build steps are the same as above with an additional cross compilation configuration `--cross-file`.

1. Update `./crossfile/android-arm64-v8a` to point to your NDK installation.
2. An example of compiling ffmpeg libraries for Android can be found in the [rt-common-shared](https://github.com/5G-MAG/rt-common-shared/tree/main/avcodec-build) repository.

```
meson setup --wipe -Davpipeline=true -Davpipeline:avcodec_dir=external/avcodec/android/arm64-v8a build/android-aarch64 --cross-file ./crossfile/android-arm64-v8a
meson compile -C ./build/android-aarch64
```


## Hardware video decoding

Video streams are decoded with the hardware decoders of the platform when they support the stream, and with the FFmpeg software decoders otherwise:

| Platform | Hardware decoder |
|---|---|
| macOS, iOS | VideoToolbox |
| Android | MediaCodec (H.264, HEVC, VP8, VP9, AV1, MPEG-4 Part 2, MPEG-2), through the NDK API |

The decoded pictures are copied to the texture buffers in the format the buffer requests, as with the software decoders.

The `hwdecode` option controls it at build time: `auto` (default) uses the hardware decoders of the platform when there are some, `enabled` fails when the platform has none, `disabled` only builds the software decoders.
```
meson setup -Davpipeline=true -Davpipeline:hwdecode=disabled ...
```

The `AVPIPELINE_HWDECODE` environment variable changes it at run time: `0` uses the software decoders only; `require` fails to decode a stream that no hardware decoder supports (the tests use it to check the hardware decoders).


## Tests

`test/pipeline_consumer.cpp` plays clips encoded at test time through the pipeline, in the role of the Presentation Engine, and checks the frames stored in the texture buffers, including that hardware and software decoding store the same pictures. Run them with `meson test -C <builddir>`.

When cross-compiling, build them with `-Davpipeline:tests=enabled`, then run them on the device, e.g. on Android:
```
adb push <builddir>/subprojects/avpipeline/test/pipeline_consumer <builddir>/subprojects/*/lib*.so <avcodec_dir>/lib/*.so /data/local/tmp/
adb shell "cd /data/local/tmp && LD_LIBRARY_PATH=. TMPDIR=. ./pipeline_consumer"
```
