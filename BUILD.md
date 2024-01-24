<!-- TOC -->

- [Build](#build)
    - [Release builds](#release-builds)
    - [Build system](#build-system)
    - [Dependencies](#dependencies)
        - [FFmpeg](#ffmpeg)
        - [Catch2](#catch2)
    - [configure targets](#configure-targets)
            - [libmaf](#libmaf)
            - [csharp](#csharp)
            - [avpipeline](#avpipeline)
    - [Windows](#windows)

<!-- /TOC -->

# Build

Providing you have all [dependencies installed](#Dependencies), the project can be built and installed on windows as follow:

```
git clone https://github.com/5G-MAG/rt-xr-maf-native.git
cd rt-xr-maf-native
meson setup builddir
meson compile -C builddir
meson install -C builddir --tags swig_cs --destdir path\to\rt-xr-unity-player\Packages\rt.xr.maf
meson install -C builddir --tags libmaf_cs,avpipeline --destdir path\to\rt-xr-unity-player\Packages\rt.xr.maf\x86_64-w64
```

## Release builds

By default, the meson setup configures a debug build:
```
meson setup builddir
```

Switch to a release build after setup:
```
meson configure --buildtype=release builddir
```


## Build system

The project uses the meson build system. See [this quickstart guide](https://mesonbuild.com/Quick-guide.html#compiling-a-meson-project) if you are new to it.


## Dependencies

`meson` will use dependencies available on the system, else will fallback to custom dependency definitions. Make sure to [check out meson's documentation](https://mesonbuild.com/Dependencies.html#declaring-your-own) if you are new to meson.

If a dependency is not detected, the `meson setup` command looks for a fallback subproject, and will download automatically the ressource specfied in dependency's wrap files.

When configuring the project use the [--wrap-mode](https://mesonbuild.com/Subprojects.html#commandline-options) option to control the dependency lookup behavior.


### FFmpeg

The project has a dependency on FFmpeg 5.1 libraries.
See [subprojects/packagefiles/FFmpeg-5.1/README.md](subprojects/packagefiles/FFmpeg-5.1/README.md) for guidance on setting it up.

### Catch2

The Catch2 is used for unit tests and is downloaded automatically upon setup, as specified in [`subprojects/catch2.wrap`](subprojects/catch2.wrap).

to disable automatic download 
```
meson setup --wrap-mode=nodownload builddir
```



## configure targets

All build options with their default values are specified in `meson_options.txt`.

#### libmaf

`libmaf` provides the core MAF API and media pipeline factory.
```
meson setup -Dlibmaf=true -Dcsharp=false -Davpipeline=false builddir
meson compile -C builddir
```

#### csharp

`csharp` priovides the C# bindings for `libmaf`, composed of .cs source files and a lightweight C++ wrapper. These are generated using [SWIG](https://swig.org/). The files in `subprojects/maf_csharp` should be re-generated whenever the API changes.

See [subprojects/maf_csharp/README.md](subprojects/maf_csharp/README.md) for details.

```
meson setup --reconfigure -Dcsharp=true -Davpipeline=false -Dlibmaf=false builddir
meson compile -C builddir
```

#### avpipeline

`avpipeline` is a media pipeline plugin implemented using libav.

```
meson setup --reconfigure -Davpipeline=true -Dlibmaf=false -Dcsharp=false builddir
meson compile -C builddir
```


## Windows 

On windows, in order to build using the visual studio compiler and linker, run commands from a 'Developer Powershell VisualStudio' console.
See also: [https://mesonbuild.com/Using-with-Visual-Studio.html](https://mesonbuild.com/Using-with-Visual-Studio.html)

Alternatively, the project can be built with `gcc` using MSYS2.