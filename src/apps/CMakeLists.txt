cmake_minimum_required(VERSION 3.27)

add_library(app INTERFACE app.hpp)
target_compile_options(app INTERFACE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(app INTERFACE board error)

add_subdirectory(blinky)
add_subdirectory(uart_echo)
add_subdirectory(i2c_test)