# test media (test_media.hpp), for the end-to-end tests
avpipeline_test_includes = include_directories('.')

# Consumer tests: the test plays the role of the Presentation Engine against AvMediaPipeline. The test media is
# encoded at test time with the libav libraries, so the tests only run where the host can run the binaries.
pipeline_consumer = executable(
    'pipeline_consumer',
    files(['pipeline_consumer.cpp']),
    include_directories : [avpipeline_includes, include_directories('.'), subproject('maf').get_variable('maf_test_includes')],
    cpp_args : avpipeline_args,
    link_with : avpipeline.get_shared_lib(),
    dependencies : [libmaf_dep.as_shared(), libav_deps, platform_deps]
)
test('pipeline_consumer', pipeline_consumer, timeout : 120)
