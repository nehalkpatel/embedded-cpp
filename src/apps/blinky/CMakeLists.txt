add_executable(blinky blinky.cpp)
target_link_libraries(blinky PRIVATE project_options app platform_entry mcu error)
