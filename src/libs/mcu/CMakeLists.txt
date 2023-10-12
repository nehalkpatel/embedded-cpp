cmake_minimum_required(VERSION 3.27)

add_library(mcu INTERFACE pin.hpp i2c.hpp) # pin.cpp i2c.cpp)
target_compile_options(mcu INTERFACE ${COMMON_COMPILE_OPTIONS})
target_include_directories(mcu INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mcu INTERFACE error)

add_subdirectory(${EMBEDDED_CPP_MCU})