<h1 align="center">XR Player - Media pipelines plugins</h1>
<p align="center">
  <img src="https://img.shields.io/badge/Status-Under_Development-yellow" alt="Under Development">
  <img src="https://img.shields.io/github/v/tag/5G-MAG/rt-xr-maf-native?label=version" alt="Version">
  <img src="https://img.shields.io/badge/License-5G--MAG%20Public%20License%20(v1.0)-blue" alt="License">
</p>

## Introduction

This repository provides **media pipelines** implementations supporting the 5G-MAG Reference Tools [XR player](https://github.com/5G-MAG/rt-xr-unity-player) Unity 3D project.

Media pipelines are plugins for the XR Player's media player implementation of the Media Access Functions API (*MAF*) defined in [ISO/IEC 23090-14](https://www.iso.org/obp/ui/#iso:std:iso-iec:23090:-14:ed-1:v1:en). 

The *MAF API* abstracts timed media fetching, exposing decoded timestamped media buffers to the 3D presentation engine.

The media buffers may provide video, audio, but also 3D geometry arrays or arbitrary data referenced described by accessor headers.

The project is composed of the following libraries:
- **subprojects/maf**: the MAF API implemented by media pipeline plugins - [documentation](docs/maf.md)
- **subprojects/maf_csharp**: C# bindings for the MAF API - [documentation](docs/csharp_bindings.md)

*Media pipeline plugins:*
- **subprojects/avpipeline**: a generic media pipeline using *libav* for Audio/Video decoding.
- [Writing a new media pipeline plugin](docs/plugins.md)

## Build and install using docker

```
# Get the source
git clone https://github.com/5G-MAG/rt-xr-maf-native.git 

# Build everything into a docker container
cd rt-xr-maf-native
docker build -t maf:builder .

# Copy build artifacts into a local directory
docker run --mount=type=bind,source=$(pwd)/Packages/rt.xr.maf,target=/install -it maf:builder
```

**Note for ARM users (eg. Apple silicon) :** 

The docker image uses `platform=linux/amd64` because Android NDK doesn't support ARM on Linux. 
To work around this limitation the following solution can be used :
- use [podman](https://podman.io/) instead of docker
- use `podman run` [--arch](https://docs.podman.io/en/latest/markdown/podman-run.1.html#arch-arch) option : `run --arch=amd64 --mount=type=bind,source=$(pwd)/Packages/rt.xr.maf,target=/install -it maf:builder`


## 1. Cloning

```
git clone https://github.com/5G-MAG/rt-xr-maf-native.git
```

## 2. Setup dependencies

### Compiling for Android

Android API, NDK and SDK version should match the [environment setup of the Unity 3D editor's Android build support module](https://docs.unity3d.com/Manual/android-sdksetup.html).

The Android build support module installed by Unity 2022 - using NDK version 23.2.8568313 - is known to work as configured in the `crossfile/android-aarch64` file.


### meson build system

The project uses the [meson build system](https://mesonbuild.com/Quick-guide.html). It can be conveniently installed in a python virtual environment using : `./scripts/install_meson.sh`


### Plugin specific dependencies

#### FFmpeg 7.1

The `subproject/avpipeline` plugin has a dependency on libav. When compiling the plugin meson first attempts to look for system dependency, then looks for FFmpeg as a subproject providing the dependencies.

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

> The `./scripts/build_ffmpeg_android.sh` uses a third party project [ffmpeg-android-maker](https://github.com/Javernaut/ffmpeg-android-maker) to compile ffmpeg for android and setup the dependency subproject. Please check the script in order to download the dependency in the right path.


## 3. Compile all libraries and plugins

Before compiling, edit the [crossfile](https://mesonbuild.com/Cross-compilation.html#cross-file-locations) configuration to point to your Android NDK toolchain installation's.

```
./scripts/build_android.sh
```

the script generates a **debug** build.

## 4. Copy libraries, plugins, and dependencies to the Unity project

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

