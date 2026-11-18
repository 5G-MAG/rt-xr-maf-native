# Consumer tests: the test plays the role of the Presentation Engine against the Buffer API
buffer_consumer = executable(
    'buffer_consumer',
    files(['buffer_consumer.cpp']),
    include_directories : libmaf_includes,
    link_with : libmaf
)
test('buffer_consumer', buffer_consumer)
