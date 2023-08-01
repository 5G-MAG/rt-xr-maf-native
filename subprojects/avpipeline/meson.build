project(
  'avpipeline',
  'cpp',
  version : '0.0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17']
)

cxx = meson.get_compiler('cpp')

libmaf_dep = dependency('libmaf', method: 'auto', fallback: ['maf', 'libmaf_dep'])

# libav dependency taken from gstreamer's libav plugin build file
# this loads system dependcy if it exists, otherwise fallbacks to Subprojects/FFmpeg/meson.build
libavformat_dep = dependency('libavformat', method: 'auto', version: '>= 59.27.100',
  fallback: ['FFmpeg', 'libavformat_dep'])
libavcodec_dep = dependency('libavcodec', method: 'auto', version: '>= 59.37.100',
  fallback: ['FFmpeg', 'libavcodec_dep'])
libavutil_dep = dependency('libavutil', method: 'auto', version: '>= 57.28.100',
  fallback: ['FFmpeg', 'libavutil_dep'])
libswscale = dependency('libswscale', method: 'auto', version: '>= 6.7.100',
  fallback: ['FFmpeg', 'libswscale_dep'])
libswresample = dependency('libswresample', method: 'auto', version: '>= 4.7.100',
  fallback: ['FFmpeg', 'libswresample_dep'])

libav_deps = [libavformat_dep, libavcodec_dep, libavutil_dep, libswscale, libswresample]

libav_deps_type_name = ''

foreach dep: libav_deps
  if libav_deps_type_name != '' and dep.type_name() != libav_deps_type_name
    error('Libav deps must be either all internal or all external')
  endif
  libav_deps_type_name = dep.type_name()
endforeach

avpipeline_sources = files(['src/AvPipeline.cpp', 'src/Decoder.cpp'])
avpipeline_includes = include_directories('include')

avpipeline = shared_library(
    'avpipeline',
    avpipeline_sources,
    gnu_symbol_visibility : 'default',
    include_directories : [avpipeline_includes],
    dependencies: [libmaf_dep, libav_deps],
    cpp_args : ['-D__MAFAPI_EXPORT'],
    install: true
)