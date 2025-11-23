cmake_minimum_required(VERSION 3.27)

add_executable(uart_echo uart_echo.cpp)
target_compile_options(uart_echo PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(uart_echo PRIVATE error sys mcu)
