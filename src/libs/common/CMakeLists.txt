cmake_minimum_required(VERSION 3.27)

add_library(error INTERFACE error.hpp)
target_compile_options(error INTERFACE ${COMMON_COMPILE_OPTIONS})
set_target_properties(error PROPERTIES LINKER_LANGUAGE CXX)

add_library(logger logger.hpp logger.cpp)
target_compile_options(logger PRIVATE ${COMMON_COMPILE_OPTIONS})
target_include_directories(logger PUBLIC ${CMAKE_SOURCE_DIR}/src)
