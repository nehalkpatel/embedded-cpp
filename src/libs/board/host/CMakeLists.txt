cmake_minimum_required(VERSION 3.27)

add_library(host_board host_board.hpp host_board.cpp)
target_compile_options(host_board PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(host_board PRIVATE board mcu host_mcu cppzmq nlohmann_json::nlohmann_json)

add_library(sys main.cpp)
target_compile_options(sys PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(sys PRIVATE app board mcu host_board host_mcu cppzmq)
