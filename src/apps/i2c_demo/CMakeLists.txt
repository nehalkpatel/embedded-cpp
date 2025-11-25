cmake_minimum_required(VERSION 3.27)

add_executable(i2c_demo i2c_demo.cpp)
target_compile_options(i2c_demo PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(i2c_demo PRIVATE error sys mcu)
