cmake_minimum_required(VERSION 3.27)

add_library(app INTERFACE app.hpp)
target_compile_options(app INTERFACE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(app INTERFACE board)

add_subdirectory(blinky)