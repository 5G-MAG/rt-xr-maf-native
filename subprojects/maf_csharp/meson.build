project(
  'maf_cs',
  'cpp',
  version : '0.0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17']
)

libmaf_dep = dependency('maf', method: 'auto', fallback: ['maf', 'libmaf_dep'])

cs_wrapper_sources = files(['maf.cpp'])
cs_wrapper = both_libraries(
    'maf_cs',
    cs_wrapper_sources,
    gnu_symbol_visibility : 'default',
    dependencies: [libmaf_dep],
    install : true,
    cpp_args : ['-D__MAFAPI_EXPORT']
)

cs_dir = meson.current_source_dir()
install_subdir(join_paths(cs_dir, 'maf'), install_dir: 'maf', install_tag: 'cs', strip_directory: true)