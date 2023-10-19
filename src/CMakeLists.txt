cmake_minimum_required(VERSION 3.27)

# include files via relative path from src
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(apps)
add_subdirectory(libs)