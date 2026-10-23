<p align="center">
  <img src=".github/banner.svg" width="100%" alt="Reference Tools · XR/3D Scenes with MPEG-I Scene Description: XR Media Pipelines">
</p>

<p align="center">
  Media pipeline plugins for the <a href="https://github.com/5G-MAG/rt-xr-unity-player">XR Unity Player</a>,
  built on the Media Access Functions (MAF) API of MPEG-I Scene Description (ISO/IEC 23090-14).
</p>

<p align="center">
  <img alt="Status" src="https://img.shields.io/endpoint?url=https%3A%2F%2Fwww.5g-mag.com%2Fbadges%2Fstatus%2Frt-xr-maf-native.json">
  <a href="https://github.com/5G-MAG/rt-xr-maf-native/releases"><img alt="Version"
    src="https://img.shields.io/github/v/release/5G-MAG/rt-xr-maf-native?label=Version"></a>
  <a href="LICENSE"><img alt="License: 5G-MAG Public License v1.0"
    src="https://img.shields.io/badge/License-5G--MAG%20PL%20v1.0-blue"></a>
</p>

<p align="center">
  <a href="https://www.5g-mag.com/reference-tools/xr/">Project page</a> &nbsp;&middot;&nbsp;
  <a href="https://github.com/5G-MAG/rt-xr-maf-native/issues">Issues</a> &nbsp;&middot;&nbsp;
  <a href="https://www.5g-mag.com/contributing">Contributing</a>
</p>

---

## At a glance

|  |  |
|---|---|
| **Implements** | Media Access Functions API (*MAF*) of [ISO/IEC 23090-14](https://www.iso.org/obp/ui/#iso:std:iso-iec:23090:-14:ed-1:v1:en), edition 1 as linked (no date stated) |
| **Part of** | [XR/3D Scenes with MPEG-I Scene Description](https://www.5g-mag.com/reference-tools/xr/), alongside [rt-xr-blender-exporter](https://github.com/5G-MAG/rt-xr-blender-exporter), [rt-xr-content](https://github.com/5G-MAG/rt-xr-content), [rt-xr-gITFast](https://github.com/5G-MAG/rt-xr-gITFast), [rt-xr-unity-player](https://github.com/5G-MAG/rt-xr-unity-player) |

## Introduction

This repository provides the media pipelines that the
[XR Unity Player](https://github.com/5G-MAG/rt-xr-unity-player) uses to fetch and decode media for
a glTF scene. It builds the MAF library, its C# bindings for Unity, and the pipeline plugins that
implement the API.

The *MAF API* abstracts timed media fetching: it exposes decoded, timestamped media buffers to the
3D presentation engine, and the presentation engine updates the media pipelines with the viewer's
pose and viewport information.

The media buffers can carry video and audio, but also 3D geometry arrays or arbitrary data,
referenced and described through buffer accessors. A glTF document signals the media pipeline
configuration through the
[MPEG_media](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/MPEG_media),
[MPEG_buffer_circular](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/MPEG_buffer_circular)
and [MPEG_accessor_timed](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/MPEG_accessor_timed)
glTF extensions.

### Overview

The repository uses the [meson build system](https://mesonbuild.com/) and is made of several
[meson subprojects](https://mesonbuild.com/Subprojects.html).

### MAF library and API

- **subprojects/maf**: the MAF API that media pipeline plugins implement ([documentation](docs/maf.md))
- **subprojects/maf_csharp**: C# bindings for the MAF API ([documentation](docs/maf_csharp.md))

### Media pipeline plugins

- **subprojects/avpipeline**: a generic media pipeline for audio and video decoding

Media pipeline plugins implement the MAF API and extend the XR client application with further
media decoding capabilities. See [Writing a new media pipeline plugin](docs/plugins.md).

## Specification

The MAF API is defined in ISO/IEC 23090-14. The README links edition 1 of the document and states
no date.

Clause-by-clause coverage, and what is still absent, is recorded on the project page rather than
here: <https://www.5g-mag.com/reference-tools/xr/>

## Install dependencies

### Build system

The project uses the [meson](https://mesonbuild.com/) build system. See the "Build system" section of
[docs/BUILD.md](docs/BUILD.md#build-system) to install meson.

## Downloading

```
git clone https://github.com/5G-MAG/rt-xr-maf-native.git
```

## Building

### Building the MAF library and C# bindings

Set up and compile the project:
```
meson setup --wipe -Ddebug=true builddir
meson compile -C builddir
```

The MAF library and the C# bindings are built by default; the avpipeline plugin is not (see
[Building media pipeline plugins](#building-media-pipeline-plugins)).

#### Compiling for Android

The build steps are the same as above, with an additional cross-compilation configuration.

1. Review the cross compilation configuration file `./crossfile/android-arm64-v8a` and modify the toolchain paths to point to local Android NDK.
2. Set up the project for cross compilation with the `--cross-file` option.

```
meson setup --wipe ./build/arm64-v8a --cross-file ./crossfile/android-arm64-v8a
meson compile -C ./build/arm64-v8a
```

As committed, the cross file points at a Windows installation of Android NDK 28.0.12674087.

### Building media pipeline plugins

Each plugin's own README has its build instructions:
* [avpipeline](subprojects/avpipeline/README.md)

## Development

### Gitflow

This project follows the [Gitflow workflow](https://www.atlassian.com/git/tutorials/comparing-workflows/gitflow-workflow).
The `development` branch is the integration branch for new features, so switch to the
`development` branch before starting work on a new feature.

## Contributing

Contributions are welcome. How to raise an issue, fork the repository and open a pull request, and
the Contributor License Agreement required before code can be merged, are described at
<https://www.5g-mag.com/contributing>.

## License

Distributed under the 5G-MAG Public License v1.0. See [LICENSE](LICENSE).
