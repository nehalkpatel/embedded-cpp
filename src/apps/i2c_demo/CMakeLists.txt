add_executable(i2c_demo i2c_demo.cpp)
target_link_libraries(i2c_demo PRIVATE project_options app sys mcu error)
