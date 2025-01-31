<h1 align="center">XR Player - Media pipelines plugins</h1>
<p align="center">
  <img src="https://img.shields.io/badge/Status-Under_Development-yellow" alt="Under Development">
  <img src="https://img.shields.io/github/v/tag/5G-MAG/rt-xr-maf-native?label=version" alt="Version">
  <img src="https://img.shields.io/badge/License-5G--MAG%20Public%20License%20(v1.0)-blue" alt="License">
</p>

## Introduction

This repository provides **media pipelines** implementations supporting the 5GMAG reference tools [XR player](https://github.com/5G-MAG/rt-xr-unity-player) Unity 3D project.

Media pipelines are plugins for the XR Player's media player implementation implementing the Media Access Functions API (*MAF*) defined in [ISO/IEC 23090-14](https://www.iso.org/obp/ui/#iso:std:iso-iec:23090:-14:ed-1:v1:en). 

The *MAF API* abstracts timed media fetching, exposing decoded timestamped media buffers to the 3D presentation engine.

The media buffers may provide video, audio, but also 3D geometry arrays or arbitrary data referenced described by accessor headers.

The project is composed of the following libraries:
- **subprojects/maf**: the MAF API implemented by media pipeline plugins - [documentation](docs/maf.md)
- **subprojects/maf_csharp**: C# bindings for the MAF API - [documentation](docs/csharp_bindings.md)

*Media pipeline plugins:*
- **subprojects/avpipeline**: a generic media pipeline using *libav* for Audio/Video decoding.
- [Writing a new media pipeline plugin](docs/plugins.md)


## 1. cloning

```
git clone https://github.com/5G-MAG/rt-xr-maf-native.git
```

## 2. setup dependencies

### compiling for Android

Android API, NDK and SDK version should match the [environment setup of the Unity 3D editor's Android build support module](https://docs.unity3d.com/Manual/android-sdksetup.html).

The Android build support module installed by Unity 2022 - using NDK version 23.2.8568313 - is known to work as configured in the `crossfile/android-aarch64` file.


### meson build system

The project uses the [meson build system](https://mesonbuild.com/Quick-guide.html). It can be conveniently installed in a python virtual environment using : `./scripts/install_meson.sh`


### plugin specific dependencies

#### FFmpeg 7.1

the `subproject/avpipeline` plugin has a dependency on libav. When compiling the plugin meson first attempts to look for system dependency, then looks for FFmpeg as a subproject providing the dependencies.

For Android, the precompiled FFmpeg libraries and headers should be copied to the dedicated subproject:
```
subprojects/
  avpipeline/
  FFmpeg/
    meson.build
    arm64/
      include/
      lib/
  maf/
  maf_csharp/
```

The meson.build file should be copied from the script directory:
`cp ./scripts/build_ffmpeg_android.meson ./subprojects/FFmpeg/meson.build`

> The `./scripts/build_ffmpeg_android.sh` uses a third party project [ffmpeg-android-maker](https://github.com/Javernaut/ffmpeg-android-maker) to compile ffmpeg for android and setup the dependency subproject.


## 3. compile all libraries and plugins

Before compiling, edit the [crossfile](https://mesonbuild.com/Cross-compilation.html#cross-file-locations) configuration to point to your Android NDK toolchain installation's.

```
./scripts/build_android.sh
```

the script generates a **debug** build.

## 4. copy libraries, plugins, and dependencies to the Unity project

Run the installation script for android, passing installation directory as an argument:
```
./scripts/install_android.sh ../rt-xr-unity-player/Packages/rt.xr.maf
```


### Gitflow

This project follows the [Gitflow workflow](https://www.atlassian.com/git/tutorials/comparing-workflows/gitflow-workflow). 

The `development` branch of this project serves as an integration branch for new features. 

Consequently, please make sure to switch to the `development` branch before starting the implementation of a new feature.


### Licenses

This project is provided under 5G-MAG's Public License. For the full license terms, please see the LICENSE file distributed along with the repository or retrieve it from [here](https://drive.google.com/file/d/1cinCiA778IErENZ3JN52VFW-1ffHpx7Z/view).

