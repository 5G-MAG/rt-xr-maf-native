project(
  'maf',
  'cpp',
  version : '0.0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17', 'werror=true']
)

project_description = 'C++ implementation of the Media Access Functions API defined in ISO/IEC 23090-14'


libmaf_sources = files(['src/maf.cpp',  'src/factory.cpp', 'src/utils.cpp'])
libmaf_includes = include_directories('include')

# Native frame buffers (implementation convention, see docs/maf.md): Frame.data of a native buffer is a CVPixelBufferRef
# on Apple platforms and an AHardwareBuffer* on Android. Other platforms have no native buffers.
native_frames = get_option('native_frames') == 'auto' and host_machine.system() in ['darwin', 'android']
message('native frame buffers: @0@'.format(native_frames))
subdir('include')
libmaf = both_libraries(
    'maf',
    libmaf_sources,
    cpp_shared_args : [ '-DMAF_API_EXPORT' ],
    include_directories : libmaf_includes,
    gnu_symbol_visibility : 'default',
    pic : true,
    install_tag: 'libmaf'
)

libmaf_dep = declare_dependency(include_directories: libmaf_includes, link_with: libmaf, sources: maf_config)

subdir('test')