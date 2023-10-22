cmake_minimum_required(VERSION 3.27)

add_library(mcu INTERFACE pin.hpp i2c.hpp delay.hpp)
target_compile_options(mcu INTERFACE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(mcu INTERFACE error)

add_subdirectory(${EMBEDDED_CPP_MCU})
