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

avpipeline_proj = disabler()
if get_option('avpipeline')
    avpipeline_proj = subproject('avpipeline')
endif

maf_unity_proj = disabler()
if not get_option('unityplugin').disabled()
    maf_unity_proj = subproject('maf_unity', required : get_option('unityplugin'))
endif

if get_option('csharp')
    subproject('maf_csharp')
endif

# end-to-end consumer tests: frames of the hardware decoders shown through the maf_unity plugin
if get_option('avpipeline') and maf_unity_proj.found() and meson.can_run_host_binaries() and avpipeline_proj.get_variable('use_hwdecode')
    subdir('test')
endif
