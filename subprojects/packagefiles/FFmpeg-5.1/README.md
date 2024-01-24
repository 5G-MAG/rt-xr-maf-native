
# FFmpeg dependency resolution

The exact FFmpeg version expected is defined in [subprojects/packagefiles/FFmpeg-5.1/meson.build file](meson.build).

Dependency resolution happens in the following order:

1. `meson setup` [detects if dependencies are installed on the system](https://mesonbuild.com/Dependencies.html#dependency-detection-method) using pkgconfig by default.

2. if a dependency is missing, meson looks for `subprojects/FFmpeg-5.1` declaring the dependency : [instructions](#bring-your-own-precompiled-binaires).

3. still is missing ? it looks for a `subprojects/FFmpeg.wrap` file to download an archive containing a meson project declaring the dependecy : [instructions](#download-precompiled-libraries-automatically-on-project-configuration).


## bring your own precompiled libraries

1. Create the `subprojects/FFmpeg/` directory.
2. Copy `subprojects/packagefiles/FFmpeg-5.1/meson.build`
3. Copy precompiled libraries:
    ```
    subprojects/FFmpeg/bin/
        avcodec-59.dll
        avdevice-59.dll
        avfilter-8.dll
        avformat-59.dll
        avutil-57.dll
        postproc-56.dll
        swresample-4.dll
        swscale-6.dll
    subprojects/FFmpeg/include/
        libavcodec/*.h
        libavdevice/*.h
        libavfilter/*.h
        libavformat/*.h
        libavutil/*.h
        libpostproc/*.h
        libswresample/*.h
        libswscale/*.h
    subprojects/FFmpeg/lib/
        pkgconfig
        avcodec-59.def
        avcodec.lib
        avdevice-59.def
        avdevice.lib
        avfilter-8.def
        avfilter.lib
        avformat-59.def
        avformat.lib
        avutil-57.def
        avutil.lib
        libavcodec.dll.a
        libavdevice.dll.a
        libavfilter.dll.a
        libavformat.dll.a
        libavutil.dll.a
        libpostproc.dll.a
        libswresample.dll.a
        libswscale.dll.a
        postproc-56.def
        postproc.lib
        swresample-4.def
        swresample.lib
        swscale-6.def
        swscale.lib
    subprojects/FFmpeg/LICENSE.txt
    ```


### building ffmpeg

One convenient way to build ffmpeg on windows is to use [https://github.com/BtbN/FFmpeg-Builds](github.com/BtbN/FFmpeg-Builds),
the repository is listed on [ffmpeg.org](https://ffmpeg.org/download.html#build-windows):
```
git clone https://github.com/BtbN/FFmpeg-Builds.git 
cd FFmpeg-Builds
./makeimage.sh win64 gpl-shared 5.1
./build.sh target win64 gpl-shared 5.1
ls -l artifacts/
```


## download precompiled libraries automatically on project configuration

It is possible to automatically download pre-built libraries over HTTP when running `meson build`.

1. create a wrap file describing were to get the pre-compiled libraries for ffmpeg 5.1, **subprojects/FFmpeg.wrap**:
```
[wrap-file]
directory = ffmpeg-n5.1-latest-win64-gpl-shared-5.1
source_url = https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n5.1-latest-win64-gpl-shared-5.1.zip
source_filename = ffmpeg-n5.1-latest-win64-gpl-shared-5.1.zip
source_hash = 8c838f5ae92933ac55293b19854ef158c0dcdcf1dbf164df211803e2b76281b8
patch_directory = FFmpeg-5.1

[provide]
dependency_names = avformat, libavutil, libavcodec, libswscale, swresample
```
*note: the `source_hash` may be outdated in the above exemple.*

2. do **not** remove `subprojects/packagefiles/FFmpeg-5.1`

3. when running `meson setup` the dependency described in the `FFmpeg.wrap` file will be downloaded from `https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n5.1-latest-win64-gpl-shared-5.1.zip`
