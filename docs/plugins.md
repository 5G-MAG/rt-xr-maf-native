
> # Info
> This section is a *work in progress* intended to be contributed to the [XR section of 5G-MAG's Getting-Started website](https://5g-mag.github.io/Standards/pages/xr.html).


# Creating a custom MediaPipeline plugin for use in the XR Player

A media pipeline implements media fetching and decoding, exposing its functionalities through the [MAF API](./MAF.md).

## MediaPipeline project structure
```
- MediaPipeline
    - include
        MediaPipeline.h
    - src
        MediaPipeline.c
    meson.build
```

## Implementing the IMediaPipeline interface

- review of the `IMediaPipeline` interface:
    - `include/MediaPipeline.h`
    - `src/MediaPipeline.cpp`

- Enable registration of the `MediaPipeline` as a plugin of the `MediaPipelineFactory`
- Register the plugin in the XR Player Unity project

## Compiling the MediaPipeline as a meson subproject
- `MediaPipeline/meson.build`
- `meson.build`
- `meson.options`

