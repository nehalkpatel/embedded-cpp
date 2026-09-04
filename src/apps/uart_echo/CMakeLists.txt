add_executable(uart_echo uart_echo.cpp)
target_link_libraries(uart_echo PRIVATE project_options app platform_entry mcu error)
