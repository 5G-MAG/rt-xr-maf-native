project('ffmpeg libraries', 'cpp')

# intended to use pre-built binaries rather than building libav from scratch
# see: https://mesonbuild.com/Shipping-prebuilt-binaries-as-wraps.html

ffmpeg_include = include_directories('include')
ffmpeg_lib_dir = [
    join_paths(meson.current_source_dir(), 'lib'), 
    join_paths(meson.current_source_dir(), 'bin')
]

cxx = meson.get_compiler('cpp')
libavformat_dep = declare_dependency(
    dependencies : cxx.find_library('avformat', required: true, dirs : ffmpeg_lib_dir), 
    include_directories : ffmpeg_include, 
    version: '59.27.100'
)

libavutil_dep = declare_dependency( 
    dependencies : cxx.find_library('avutil', required: true, dirs : ffmpeg_lib_dir ), 
    include_directories : ffmpeg_include, 
    version: '57.28.100'
)

libavcodec_dep = declare_dependency( 
    dependencies : cxx.find_library('avcodec', required: true, dirs : ffmpeg_lib_dir ), 
    include_directories : ffmpeg_include, 
    version: '59.37.100'
)

libswscale_dep = declare_dependency( 
    dependencies : cxx.find_library('swscale', required: true, dirs : ffmpeg_lib_dir ), 
    include_directories : ffmpeg_include, 
    version: '6.7.100'
)

libswresample_dep = declare_dependency( 
    dependencies : cxx.find_library('swresample', required: true, dirs : ffmpeg_lib_dir ), 
    include_directories : ffmpeg_include, 
    version: '4.7.100'
)