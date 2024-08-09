project(
  'maf',
  'cpp',
  version : '0.0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17', 'werror=true']
)

project_description = 'C++ implementation of the Media Access Functions API defined in ISO/IEC 23090-14'

build_args = [
  '-DPROJECT_NAME=' + meson.project_name(),
  '-DPROJECT_VERSION=' + meson.project_version(),
]

libmaf_sources = files(['src/maf.cpp', 'src/utils.cpp', 'src/factory.cpp'])
libmaf_includes = include_directories('include')
libmaf = both_libraries(
    'maf',
    libmaf_sources,
    c_args : build_args,
    include_directories : libmaf_includes,
    gnu_symbol_visibility : 'default',
    pic : true,
    cpp_args : ['-D__MAF_API_EXPORT'],
    install : get_option('libmaf'),
    install_tag: 'libmaf'
)

libmaf_dep = declare_dependency(include_directories: libmaf_includes, link_with: libmaf.get_static_lib())

subdir('test')