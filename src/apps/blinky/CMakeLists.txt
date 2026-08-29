add_executable(blinky blinky.cpp)
target_link_libraries(blinky PRIVATE project_options app sys mcu error)
