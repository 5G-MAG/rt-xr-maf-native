maf_conf = configuration_data()
if native_frames
  maf_conf.set('MAF_NATIVE_FRAMES', 1)
endif
maf_config = configure_file(input : 'maf_config.hpp.in', output : 'maf_config.hpp', configuration : maf_conf)
