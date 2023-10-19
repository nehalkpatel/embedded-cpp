cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

add_library(board INTERFACE board.hpp) # board.cpp)
target_compile_options(board INTERFACE ${COMMON_COMPILE_OPTIONS})
set_target_properties(board PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(board INTERFACE mcu error)

add_subdirectory(${EMBEDDED_CPP_BOARD})