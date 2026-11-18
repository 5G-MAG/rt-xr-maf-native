# End-to-end consumer tests, built when avpipeline and maf_unity are
add_languages('objcpp', native : false)
e2e_consumer = executable(
    'e2e_consumer',
    files(['e2e_consumer.mm']),
    include_directories : [
        avpipeline_proj.get_variable('avpipeline_includes'),
        maf_unity_proj.get_variable('maf_unity_includes'),
        maf_unity_proj.get_variable('unity_api_includes'),
        subproject('maf').get_variable('maf_test_includes'),
        avpipeline_proj.get_variable('avpipeline_test_includes'),
    ],
    cpp_args : avpipeline_proj.get_variable('avpipeline_args'),
    objcpp_args : ['-fobjc-arc'] + avpipeline_proj.get_variable('avpipeline_args'),
    link_with : [avpipeline_proj.get_variable('avpipeline').get_shared_lib(), maf_unity_proj.get_variable('maf_unity_core')],
    dependencies : [
        dependency('maf', fallback : ['maf', 'libmaf_dep']).as_shared(),
        avpipeline_proj.get_variable('libav_deps'),
        avpipeline_proj.get_variable('platform_deps'),
        maf_unity_proj.get_variable('apple_deps'),
    ]
)
test('e2e_consumer', e2e_consumer, timeout : 120)
