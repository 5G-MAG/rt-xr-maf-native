project(
  'maf',
  'cpp',
  version : '0.0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17', 'werror=true']
)

project_description = 'C++ implementation of the Media Access Functions API defined in ISO/IEC 23090-14'


libmaf_sources = files(['src/maf.cpp',  'src/factory.cpp', 'src/utils.cpp'])
libmaf_includes = include_directories('include')
libmaf = both_libraries(
    'maf',
    libmaf_sources,
    cpp_shared_args : [ '-DMAF_API_EXPORT' ],
    include_directories : libmaf_includes,
    gnu_symbol_visibility : 'default',
    pic : true,
    install_tag: 'libmaf'
)

libmaf_dep = declare_dependency(include_directories: libmaf_includes, link_with: libmaf)

# subdir('test')