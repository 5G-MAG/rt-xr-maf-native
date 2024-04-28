<h1 align="center">Media Access Function (MAF) API</h1>
<p align="center">
  <img src="https://img.shields.io/badge/Status-Under_Development-yellow" alt="Under Development">
  <img src="https://img.shields.io/github/v/tag/5G-MAG/rt-xr-maf-native?label=version" alt="Version">
  <img src="https://img.shields.io/badge/License-5G--MAG%20Public%20License%20(v1.0)-blue" alt="License">
</p>

## Introduction
This repository provides implementation of the Media Access Functions API (MAF) defined in [ISO/IEC 23090-14](https://www.iso.org/obp/ui/#iso:std:iso-iec:23090:-14:ed-1:v1:en). The Media Access Functions API allows media playback and access in the context of 3D Scene Description encoding.

The project is a dependency of [`rt-xr-unity-player`](https://github.com/5G-MAG/rt-xr-unity-player), providing the build artifacts for [`rt-xr-maf-plugin`](https://github.com/5G-MAG/rt-xr-maf-plugin).

Additional information can be found at: https://5g-mag.github.io/Getting-Started/pages/xr-media-integration-in-5g/

### About the implementation
The implementation is organized through *subprojects*:
- **subprojects/maf**: the core library implementing the MAF API, along with a media pipeline factory.
- **subprojects/maf_csharp**: SWIG C# bindings.
- **subprojects/avpipeline**: a media pipeline which uses libav for audio/video decoding.

The project currently supports win64.

Contributions to support additional platforms are welcome.

## Building

Providing you have all dependencies installed, the project can be built and installed on windows as follow:
```
git clone https://github.com/5G-MAG/rt-xr-maf-native.git
cd rt-xr-maf-native
meson setup builddir
meson install -C builddir --tags swig_cs --destdir path\to\rt-xr-unity-player\Packages\rt.xr.maf
meson install -C builddir --tags libmaf_cs,avpipeline --destdir path\to\rt-xr-unity-player\Packages\rt.xr.maf\x86_64-w64
```

Please take a look at the [build documentation](BUILD.md) page for onboarding and details about the build.


## Updating the C# SWIG wrapper for the maf API

Generating the C# bindings is documented in [subprojects/maf_csharp/README.md].

If your contribution makes changes to the public API (factory, plugins, MAF api), make sure to re-generate the SWIG wrappers.


## Contributing media pipelines

Media pipelines implement the MAF API to expose media access and decoding to a 3D presentation engine.
If you intend to develop a new media pipeline, take a look at the `subprojects/avpipeline` plugin.

- Create a new meson *subproject* containing for your plugin, and declare it in the top level `meson.build` file.
- When the XR player executes, it calls `MAF::MediaPipelineFactory::loadPluginsDir()` which looks up the given directory for dynamic libraries implementing the plugin interface `IMediaPipelineFactoryPlugin`.
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
