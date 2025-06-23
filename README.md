<h1 align="center">XR Player - Media pipelines plugins</h1>
<p align="center">
  <img src="https://img.shields.io/badge/Status-Under_Development-yellow" alt="Under Development">
  <img src="https://img.shields.io/github/v/tag/5G-MAG/rt-xr-maf-native?label=version" alt="Version">
  <img src="https://img.shields.io/badge/License-5G--MAG%20Public%20License%20(v1.0)-blue" alt="License">
</p>

## Introduction

This repository provides **media pipelines** implementations supporting the 5G-MAG Reference Tools [XR Player](https://github.com/5G-MAG/rt-xr-unity-player) Unity 3D project.

Media pipelines are plugins for the XR Player's media player implementation of the Media Access Functions API (*MAF*) defined in [ISO/IEC 23090-14](https://www.iso.org/obp/ui/#iso:std:iso-iec:23090:-14:ed-1:v1:en).

The *MAF API* abstracts timed media fetching, exposing decoded timestamped media buffers to the 3D presentation engine. The presentation engine updates media pipelines with the viewer's pose and viewport informations.

The media buffers may provide video, audio, but also 3D geometry arrays or arbitrary data referenced and described through buffer accessors. The media pipeline configuration is signaled in glTF documents through a the [MPEG_media](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/MPEG_media), [MPEG_buffer_circular](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/MPEG_buffer_circular) and [MPEG_accessor_timed](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/MPEG_accessor_timed) glTF extenstions.


### Overview

The repository uses the [meson build system](https://mesonbuild.com/) and comprises several [meson subprojects](https://mesonbuild.com/Subprojects.html).


### MAF library and API

The project is composed of the following libraries:
- **subprojects/maf**: the MAF API implemented by media pipeline plugins - [documentation](docs/maf.md)
- **subprojects/maf_csharp**: C# bindings for the MAF API - [documentation](docs/csharp_bindings.md)


### Media pipeline plugins

- **subprojects/avpipeline**: a generic media pipeline for Audio/Video decoding

Media pipeline plugins implement the MAF api and allow for extending the XR client application with additional media decoding capabilities. See [Writing a new media pipeline plugin](docs/plugins.md)


## Building

### Cloning

```
git clone https://github.com/5G-MAG/rt-xr-maf-native.git
```

### build system

The project uses the [meson](#meson-build-system) build system. See [these instructions](#docs/BUILD#meson-installation) to install meson.


### Building the MAF library and C# bindings

Setup and compile the project:
```
meson setup --wipe -Ddebug=true builddir
meson compile -C builddir
```

#### Compiling for Android

The build steps are the same as above with an additional cross compilation configuration.

1. Review the cross compilation configuration file `./crossfile/android-arm64-v8a` and modify the toolchain paths to point to local Android NDK.
2. Set up the project for cross compilation with the `--cross-file` option.

```
meson setup --wipe ./build/arm64-v8a --cross-file ./crossfile/android-arm64-v8a
meson compile -C ./build/arm64-v8a
```


### Building media pipeline plugins

Plugin specific instructions can be found in individual subprojects's README:
* [avpipeline](subprojects/avpipeline/README.md)



### Gitflow

This project follows the [Gitflow workflow](https://www.atlassian.com/git/tutorials/comparing-workflows/gitflow-workflow). 

The `development` branch of this project serves as an integration branch for new features. 

Consequently, please make sure to switch to the `development` branch before starting the implementation of a new feature.


### Licenses

This project is provided under 5G-MAG's Public License. For the full license terms, please see the LICENSE file distributed along with the repository or retrieve it from [here](https://drive.google.com/file/d/1cinCiA778IErENZ3JN52VFW-1ffHpx7Z/view).

