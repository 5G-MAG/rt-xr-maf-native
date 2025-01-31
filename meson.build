project(
  'rt-xr-maf-native',
  'cpp',
  version : '1.0.0',
  default_options : ['warning_level=3', 'cpp_std=c++17']
)

project_description = 'maf definitions, sample pipeline implementations, pipeline factory & c# bindings'

if get_option('libmaf')
  subproject('maf')
endif

if get_option('avpipeline')
    subproject('avpipeline')
endif

if get_option('csharp')
    subproject('maf_csharp')
endif
