cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

add_library(board INTERFACE board.hpp) # board.cpp)
target_compile_options(board INTERFACE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(board INTERFACE mcu error)
target_include_directories(board INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(${EMBEDDED_CPP_BOARD})