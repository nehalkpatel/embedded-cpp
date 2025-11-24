cmake_minimum_required(VERSION 3.27)

add_executable(i2c_test i2c_test.cpp)
target_compile_options(i2c_test PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(i2c_test PRIVATE error sys mcu)
