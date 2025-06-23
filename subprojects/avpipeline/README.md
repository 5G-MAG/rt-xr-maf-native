
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
