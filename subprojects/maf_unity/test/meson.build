# Consumer tests: the test plays the role of Unity against the parts of the plugin that do not depend on Unity
registry_consumer = executable(
    'registry_consumer',
    files(['registry_consumer.cpp']),
    include_directories : [maf_unity_includes, include_directories('../../maf/test')],
    link_with : maf_unity_core,
    dependencies : [libmaf_dep.as_shared()]
)
test('registry_consumer', registry_consumer)
