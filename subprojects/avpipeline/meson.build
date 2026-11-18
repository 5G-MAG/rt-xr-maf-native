project(
  'avpipeline',
  'cpp',
  version : '0.0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17']
)

libmaf_dep = dependency('maf', method: 'auto', static: false, fallback: ['maf', 'libmaf_dep'])

cxx = meson.get_compiler('cpp')
avcodec_dir = get_option('avcodec_dir')

avcodec_include = join_paths(avcodec_dir, 'include')
avcodec_lib_dir = [
  join_paths(meson.current_source_dir(), avcodec_dir, 'lib'),
  join_paths(meson.current_source_dir(), avcodec_dir, 'bin')
]

message('Looking for avpipeline deps in: @0@'.format(avcodec_dir))

libavformat_dep = declare_dependency(
  dependencies : cxx.find_library('avformat', dirs : avcodec_lib_dir, required: true),
  include_directories : include_directories(avcodec_include))
libavcodec_dep = declare_dependency(
  dependencies : cxx.find_library('avcodec', dirs : avcodec_lib_dir, required: true),
  include_directories : include_directories(avcodec_include))
libavutil_dep = declare_dependency(
  dependencies : cxx.find_library('avutil', dirs : avcodec_lib_dir, required: true),
  include_directories : include_directories(avcodec_include))
libswscale_dep = declare_dependency(
  dependencies : cxx.find_library('swscale', dirs : avcodec_lib_dir, required: true),
  include_directories : include_directories(avcodec_include))
libswresample_dep = declare_dependency(
  dependencies : cxx.find_library('swresample', dirs : avcodec_lib_dir, required: true),
  include_directories : include_directories(avcodec_include))
libav_deps = [libavformat_dep, libavcodec_dep, libavutil_dep, libswscale_dep, libswresample_dep]


# libav_deps_type_name = ''
# foreach dep: libav_deps
#   if libav_deps_type_name != '' and dep.type_name() != libav_deps_type_name
#     error('Libav deps must be either all internal or all external')
#   endif
#   libav_deps_type_name = dep.type_name()
# endforeach

avpipeline_sources = files(['src/AvPipeline.cpp', 'src/Decoder.cpp', 'src/MetadataDecoder.cpp', 'src/video/VideoDecodeBackend.cpp', 'src/video/HwAccelBackend.cpp'])

# hardware video decoders: VideoToolbox on Apple platforms; the software decoders remain the fallback
hwdecode_platforms = ['darwin']
hwdecode = get_option('hwdecode')
if hwdecode.enabled() and host_machine.system() not in hwdecode_platforms
  error('hwdecode: no hardware video decoder for @0@'.format(host_machine.system()))
endif
use_hwdecode = not hwdecode.disabled() and host_machine.system() in hwdecode_platforms
message('hardware video decoding: @0@'.format(use_hwdecode))
avpipeline_args = ['-DAVPIPELINE_HWDECODE=@0@'.format(use_hwdecode ? 1 : 0)]
avpipeline_includes = include_directories('include')

avpipeline = both_libraries(
    'avpipeline',
    avpipeline_sources,
    gnu_symbol_visibility : 'default',
    include_directories : [avpipeline_includes],
    cpp_args : avpipeline_args,
    dependencies: [libmaf_dep.as_shared(), libav_deps],
    install: true,
    install_tag: 'avpipeline'
)

if meson.can_run_host_binaries()
  subdir('test')
endif
