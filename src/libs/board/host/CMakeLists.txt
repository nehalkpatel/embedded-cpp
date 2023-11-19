cmake_minimum_required(VERSION 3.27)

include(FetchContent)

# ZeroMQ itself, so we are not dependent on the system version
# Use this to get the latest version, once the issue is fixed. TODO: pin this version.
# FetchContent_Declare(zeromq GIT_REPOSITORY https://github.com/zeromq/libzmq.git GIT_TAG master)
option(ZMQ_BUILD_TESTS OFF)
FetchContent_Declare(zeromq GIT_REPOSITORY https://github.com/nehalkpatel/libzmq.git GIT_TAG fix/4629-compile-warning)
FetchContent_MakeAvailable(zeromq)

# CPPZMQ header-only library; uses ZeroMQ
option(CPPZMQ_BUILD_TESTS OFF)
FetchContent_Declare(cppzmq GIT_REPOSITORY https://github.com/zeromq/cppzmq.git GIT_TAG v4.10.0)
FetchContent_MakeAvailable(cppzmq)

# FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json GIT_TAG v3.11.2)
FetchContent_MakeAvailable(json)

add_library(host_board host_board.hpp host_board.cpp)
target_compile_options(host_board PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(host_board PRIVATE board mcu host_mcu cppzmq nlohmann_json::nlohmann_json)

add_library(sys main.cpp)
target_compile_options(sys PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(sys PRIVATE app board mcu host_board host_mcu cppzmq)
