# rt-xr-maf-native

## Introduction

`rt-xr-maf-native` is an implementation of the Media Access Functions API defined in ISO/IEC 23090-14.

### Core MAF library

#### Specifications

The Media Access Functions API allows media playback and access in the context of 3D Scene Description encoding as specified through [ISO/IEC 23090-14](https://www.iso.org/obp/ui/#iso:std:iso-iec:23090:-14:ed-1:v1:en).

#### About the implementation

The repository implements the core MAF API specified through *ISO/IEC 23090-14*. 
The implementation is organized through *subprojects*:
- **maf**: the core maf API header and a media pipeline factory
- **maf_csharp**: SWIG C# bindings
- **avpipeline**: a sample media pipeline using libav for audio/video decoding

## Downloading

```
git clone https://github.com/5G-MAG/rt-xr-maf-native.git
```

## Building

### dependencies

The project uses the meson build system. See [this quickstart guide](https://mesonbuild.com/Quick-guide.html#compiling-a-meson-project) if you are new to it.

#### automatic dependency download

By default, the `meson setup` command [detects if dependencies are installed](https://mesonbuild.com/Dependencies.html#dependency-detection-method). 

If a dependency is not detected, the `meson setup` command looks for a fallback subproject and will download automatically the ressource specfied in dependency's wrap files.
See the `subprojects/*.wrap` files for dependencies details.
When configuring the project use the [--wrap-mode](https://mesonbuild.com/Subprojects.html#commandline-options) option to control the dependency lookup behavior.

**Disable automatic download of dependencies:**
```
meson setup --wrap-mode=nodownload builddir
```

**Force using FFmpeg libraries from subprojects, instead of libraries installed system wide:**
```
meson setup --force-fallback-for=FFmpeg builddir
```

### configure targets

All build options with their default values are specified in `meson_options.txt`.

#### libmaf

build and install libmaf only:
```
meson setup -Dlibmaf=true builddir
meson compile -C builddir
meson install -C builddir --no-rebuild --skip-subprojects catch2 --destdir /install/dir
```

when `--destdir` is omitted, installation is performed according to system defaults



#### C# bindings

The C# bindings for the MAF API are generated using [SWIG](https://swig.org/).
The files in `subprojects/maf_csharp` should be re-generated whenever the API changes.
See [subprojects/maf_csharp/README.md](subprojects/maf_csharp/README.md) for details.


build and install into the rt.xr.maf unity package:
```
meson setup --reconfigure -Dcsharp=true -Davpipeline=false -Dlibmaf=false builddir
meson install -C builddir --tags cs,runtime --destdir /path/to/unity-project/Packages/rt.xr.maf/Lib
```


#### media pipelines

build the sample media pipeline as a shared library and install it into the rt.xr.maf unity package:
```
cd rt-xr-maf-native
meson setup --reconfigure -Davpipeline=true -Dcsharp=false -Dlibmaf=false builddir
meson install -C builddir --tags runtime --destdir /path/to/unity-project/packages/rt.xr.maf/Plugins/win64
```


### building on Windows 

On windows, in order to build using the visual studio compiler and linker, run commands from a 'Developer Powershell VisualStudio' console.
See also: [https://mesonbuild.com/Using-with-Visual-Studio.html](https://mesonbuild.com/Using-with-Visual-Studio.html)


## Development

This project follows
the [Gitflow workflow](https://www.atlassian.com/git/tutorials/comparing-workflows/gitflow-workflow). 
The `development` branch of this project serves as an integration branch for new features. 
Consequently, please make sure to switch to the `development` branch before starting the implementation of a new feature.
