# TODO: update to v3.4 to disable catch2/SelfTest 
# https://github.com/catchorg/Catch2/pull/2693
catch2 = dependency('catch2-with-main', version : '3.4.0')
test_deps = [catch2, libmaf_dep]

exe = executable('test_bufferhandler', files(['test_bufferhandler.cpp']), dependencies : test_deps, install: false )
test('test buffer handler', exe)

exe = executable('test_frame', files(['test_frame.cpp']), dependencies : test_deps, install: false )
test('test frame', exe)

exe = executable('test_accessor_headers', files(['test_accessor_headers.cpp']), dependencies : test_deps, install: false, cpp_args : ['-D__MAF_API_EXPORT'])
test('test accessor header', exe)
