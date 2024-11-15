project(
  'rtxrmaf',
  'cpp',
  version : '0.0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17']
)

# libav dependency taken from gstreamer's libav plugin build file
# this loads system dependency if it exists, otherwise fallbacks to Subprojects/FFmpeg/meson.build
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


rtxrmaf_sources = files(['maf.cpp', 'src/maf.cpp',  'src/factory.cpp', 'src/utils.cpp', 'src/AvPipeline.cpp', 'src/Decoder.cpp' ])
rtxrmaf_includes = include_directories('include')
cs_wrapper = both_libraries(
    'rtxrmaf',
    rtxrmaf_sources,
    include_directories : rtxrmaf_includes,
    gnu_symbol_visibility : 'default',
    dependencies: [libav_deps],
    install : true,
    install_tag: 'librtxrmaf'
)

cs_dir = meson.current_source_dir()
install_subdir(join_paths(cs_dir, 'maf'), install_dir: 'maf', install_tag: 'rtxrmaf', strip_directory: true)
