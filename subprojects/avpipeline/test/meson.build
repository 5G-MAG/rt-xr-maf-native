# Consumer tests: the test plays the role of the Presentation Engine against AvMediaPipeline. The test media is
# encoded at test time with the libav libraries, so the tests only run where the host can run the binaries.
pipeline_consumer = executable(
    'pipeline_consumer',
    files(['pipeline_consumer.cpp']),
    include_directories : [avpipeline_includes, include_directories('../../maf/test')],
    link_with : avpipeline.get_shared_lib(),
    dependencies : [libmaf_dep.as_shared(), libav_deps]
)
test('pipeline_consumer', pipeline_consumer, timeout : 120)
