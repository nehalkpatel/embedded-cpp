cmake_minimum_required(VERSION 3.27)

add_executable(blinky blinky.cpp)
target_compile_options(blinky PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(blinky PRIVATE error sys mcu)