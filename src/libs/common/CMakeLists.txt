cmake_minimum_required(VERSION 3.27)

add_library(error INTERFACE error.hpp)

target_compile_options(error INTERFACE ${COMMON_COMPILE_OPTIONS})
set_target_properties(error PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(error INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
