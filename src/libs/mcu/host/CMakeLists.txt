cmake_minimum_required(VERSION 3.27)

add_library(host_mcu host_i2c.cpp)

target_compile_options(host_mcu PRIVATE ${COMMON_COMPILE_OPTIONS})
set_target_properties(host_mcu PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(host_mcu PRIVATE mcu cppzmq nlohmann_json::nlohmann_json)

target_include_directories(host_mcu INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
