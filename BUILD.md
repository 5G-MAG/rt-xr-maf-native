
# Build

Providing you have all [dependencies installed](#Dependencies), the project can be built and installed on windows as follow:

```
git clone https://github.com/5G-MAG/rt-xr-maf-native.git
cd rt-xr-maf-native
meson setup builddir
meson install -C builddir --tags swig_cs --destdir path\to\rt-xr-unity-player\Packages\rt.xr.maf
meson install -C builddir --tags libmaf,libmaf_cs,avpipeline --destdir path\to\rt-xr-unity-player\Packages\rt.xr.maf\x86_64-w64
```


## Build system

The project uses the meson build system. See [this quickstart guide](https://mesonbuild.com/Quick-guide.html#compiling-a-meson-project) if you are new to it.


## Dependencies

Please read how `meson setup` [detects if dependencies are installed on the system](https://mesonbuild.com/Dependencies.html#dependency-detection-method) if you are new to meson.

See the follow FFmpeg dependency documentation for a exemple using precompiled libraries: [/subprojects/packagefiles/FFmpeg-5.1](/5GMAG/rt-xr-maf-native/subprojects/packagefiles/FFmpeg-5.1/meson.build).


### FFmpeg

The project has a dependency on FFmpeg 5.1 libraries.

#### automatic dependency download

**Disable automatic download of dependencies:**
```
meson setup --wrap-mode=nodownload builddir
```


By default, the `meson setup` command [detects if dependencies are installed](https://mesonbuild.com/Dependencies.html#dependency-detection-method). 

If a dependency is not detected, the `meson setup` command looks for a fallback subproject and will download automatically the ressource specfied in dependency's wrap files.
See the `subprojects/*.wrap` files for dependencies details.
When configuring the project use the [--wrap-mode](https://mesonbuild.com/Subprojects.html#commandline-options) option to control the dependency lookup behavior.

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
meson setup --debug --reconfigure -Davpipeline=true -Dcsharp=false -Dlibmaf=false builddir
meson install -C builddir --tags runtime --destdir /path/to/unity-project/packages/rt.xr.maf/Plugins/win64
```


### building on Windows 

On windows, in order to build using the visual studio compiler and linker, run commands from a 'Developer Powershell VisualStudio' console.
See also: [https://mesonbuild.com/Using-with-Visual-Studio.html](https://mesonbuild.com/Using-with-Visual-Studio.html)

Alternatively, the project can be built with `gcc` using MSYS2.