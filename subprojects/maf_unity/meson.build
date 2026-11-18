project(
  'maf_unity',
  ['cpp'],
  version : '0.0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17']
)

# Unity native rendering plugin: shows the frames of MAF native frame buffers in Unity textures (see docs/plugins.md)

# graphics APIs: Metal on Apple platforms, Vulkan on Android
system = host_machine.system()
if system == 'darwin'
  add_languages('objcpp', native : false)
elif system != 'android'
  error('maf_unity: no graphics backend for @0@ yet'.format(system))
endif

libmaf_dep = dependency('maf', method : 'auto', static : false, fallback : ['maf', 'libmaf_dep'])
if not subproject('maf').get_variable('native_frames')
  error('maf_unity: libmaf is built without native frame buffers (-Dmaf:native_frames=none)')
endif

# The Unity Native Plugin API headers are not part of the repository (Unity Companion License): they are taken from a
# Unity installation, by default the newest Unity editor installed with Unity Hub
unity_api_dir = get_option('unity_plugin_api')
if unity_api_dir == ''
  found = run_command('sh', '-c', 'ls -d /Applications/Unity/Hub/Editor/*/Unity.app/Contents/PluginAPI 2>/dev/null | sort -V | tail -1', check : false)
  unity_api_dir = found.stdout().strip()
endif
if unity_api_dir == '' or not import('fs').is_file(unity_api_dir / 'IUnityGraphics.h')
  error('maf_unity: Unity PluginAPI headers not found, set -Dmaf_unity:unity_plugin_api=<Unity>/PluginAPI')
endif
message('Unity PluginAPI headers: @0@'.format(unity_api_dir))
unity_api_includes = include_directories(unity_api_dir)

maf_unity_includes = include_directories('include', 'src')

core_sources = files(['src/TextureRegistry.cpp'])
plugin_sources = files(['src/MafUnity.cpp'])
apple_deps = []
platform_deps = []
if system == 'darwin'
  apple_deps = dependency('appleframeworks', modules : ['Foundation', 'Metal', 'CoreVideo'])
  platform_deps = [apple_deps]
  core_sources += files(['src/MetalImporter.mm'])
  plugin_sources += files(['src/MetalBackend.mm'])
else
  # the Vulkan functions come from Unity (vkGetInstanceProcAddr): no link to the Vulkan loader
  cxx = meson.get_compiler('cpp')
  foreach lib : ['nativewindow', 'android', 'log']
    platform_deps += cxx.find_library(lib)
  endforeach
  plugin_sources += files(['src/VulkanBackend.cpp'])
endif
# Vulkan structures are initialized with their sType only, the other fields being zero
plugin_args = system == 'android' ? ['-Wno-missing-field-initializers'] : []

# the parts of the plugin that do not depend on Unity, linked by the tests
maf_unity_core = static_library(
  'maf_unity_core',
  core_sources,
  include_directories : [maf_unity_includes, unity_api_includes],
  objcpp_args : ['-fobjc-arc'],
  dependencies : [libmaf_dep.as_shared(), platform_deps],
  pic : true
)

maf_unity = shared_library(
  'maf_unity',
  plugin_sources,
  include_directories : [maf_unity_includes, unity_api_includes],
  cpp_args : plugin_args,
  objcpp_args : ['-fobjc-arc'],
  link_with : maf_unity_core,
  dependencies : [libmaf_dep.as_shared(), platform_deps],
  gnu_symbol_visibility : 'default',
  install : true,
  install_tag : 'maf_unity'
)

# the tests use Metal
if system == 'darwin' and meson.can_run_host_binaries()
  subdir('test')
endif
