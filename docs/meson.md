
`meson setup [...] builddir`: configures the project for an out of tree build in builddir
`--wipe` : wipe out builddir, ensure a clean build
`-Ddebug=true` : perform a debug build
`-Dcsharp=true` : build the C# wrapper subproject. Available options are defined in `meson_options.txt`
`--cross-file crossfile/android-aarch64` : [cross compilation](https://mesonbuild.com/Cross-compilation.html) targeting Android