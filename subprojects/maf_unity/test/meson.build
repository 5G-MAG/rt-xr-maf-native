# Consumer tests: the test plays the role of Unity against the parts of the plugin that do not depend on Unity
registry_consumer = executable(
    'registry_consumer',
    files(['registry_consumer.cpp']),
    include_directories : [maf_unity_includes, subproject('maf').get_variable('maf_test_includes')],
    link_with : maf_unity_core,
    dependencies : [libmaf_dep.as_shared()]
)
test('registry_consumer', registry_consumer)

metal_consumer = executable(
    'metal_consumer',
    files(['metal_consumer.mm']),
    include_directories : [maf_unity_includes, unity_api_includes, subproject('maf').get_variable('maf_test_includes')],
    objcpp_args : ['-fobjc-arc'],
    link_with : maf_unity_core,
    dependencies : [libmaf_dep.as_shared(), apple_deps]
)
test('metal_consumer', metal_consumer)
