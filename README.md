<h1 align="center">Media pipelines factory and plugins implementing the Media Access Function (MAF) API</h1>
<p align="center">
  <img src="https://img.shields.io/badge/Status-Under_Development-yellow" alt="Under Development">
  <img src="https://img.shields.io/github/v/tag/5G-MAG/rt-xr-maf-native?label=version" alt="Version">
  <img src="https://img.shields.io/badge/License-5G--MAG%20Public%20License%20(v1.0)-blue" alt="License">
</p>

## Introduction

This repository provides implementation of the Media Access Functions API (MAF) defined in [ISO/IEC 23090-14](https://www.iso.org/obp/ui/#iso:std:iso-iec:23090:-14:ed-1:v1:en). The Media Access Functions API allows media playback and access in the context of 3D Scene Description encoding.

Additional information can be found at [xr-player-overview](https://5g-mag.github.io/Getting-Started/pages/xr-media-integration-in-5g/usage/xr-player-overview.html).

The MAF API abstracts timed media fetching, exposing decoded timestamped media buffers to the 3D presentation engine.
The media buffers may not expose video, audio, but also 3D geometry arrays or arbitrary data referenced through gltf accessors.

The implementation is organized through *subprojects*.

### the core Media Application Function library

Located in *subprojects/maf*, this is the core library for the MAF API exposed by media pipelines.
It includes a simple factory to register and retrieve media pipelines as plugins.

### Media Pipeline plugins

Media pipeline plugins implement the MAF API to provide playback control and expose decoded buffers.

*subprojects/avpipeline* - a media pipeline which uses libav for audio/video decoding.

### C# bindings for Unity

Located in *subprojects/maf_csharp* 

This library provides SWIG C# bindings for the MAF library, supporting the use of media pipelines for Unity 3D C# scripts.



## Building

The project uses the [meson build system](https://mesonbuild.com/). 
The easiest way to get meson is to install it as a python package:
```
pip3 install --user meson
```

Providing you have all dependencies installed, the project can be built as follow:

Get the code:
```
git clone https://github.com/5G-MAG/rt-xr-maf-native.git
cd rt-xr-maf-native
```

## Building

### Building Android

Configure the project for cross-compilation:
```
meson setup --wipe -Ddebug=true -Dcsharp=true builddir --cross-file crossfile/android-aarch64
```

`meson setup [...] builddir`: configures the project for an out of tree build in builddir
`--wipe` : wipe out builddir, ensure a clean build
`-Ddebug=true` : perform a debug build
`-Dcsharp=true` : build the C# wrapper subproject. Available options are defined in `meson_options.txt`
`--cross-file crossfile/android-aarch64` : [cross compilation](https://mesonbuild.com/Cross-compilation.html) targeting Android

#### Android NDK compiler toolchain

The sample `crossfile/android-aarch64` configuration provides an example to build using the Android NDK & compiler toolchain that comes as part of a Unity3D installation.


### Building for other platforms

Configure the project for an out of tree build in the 'builddir' directory:
```
meson setup builddir
```
The target OS/architecture depends on the machine's default.


Build and install artifacts in the `dist` directory
```
meson install -C builddir --destdir dist
```

Please take a look at the [build documentation](BUILD.md) page for onboarding and details about the build such as crosscompiling using Unity Editor's Android NDK toolchain.


## Using in a Unity Project and debuging on Device

...


## Updating the C# SWIG wrapper for the maf API

Generating the C# bindings is documented in [subprojects/maf_csharp/README.md].

If your contribution makes changes to the public API (factory, plugins, MAF api), make sure to re-generate the SWIG wrappers.



## Contributing media pipelines


If you intend to develop a new media pipeline, please take a look at the `subprojects/avpipeline` plugin.

- Create a new meson *subproject* containing for your plugin, and declare it in the top level `meson.build` file.
- A call to `MediaPipelineFactory::createMediaPipeline()` iterates registered plugins until a MediaPipeline is successfully created - if any - and can returned to the application.


### Gitflow

This project follows the [Gitflow workflow](https://www.atlassian.com/git/tutorials/comparing-workflows/gitflow-workflow). 

The `development` branch of this project serves as an integration branch for new features. 

Consequently, please make sure to switch to the `development` branch before starting the implementation of a new feature.

### pre_commit hook

Contributors are encouraged to use the provided *pre_commit* hook which builds and tests before each commit.
```
cp pre_commit_hook.sh .git/hooks/pre_commit
```

### Running tests

```
meson test -C builddir
```
