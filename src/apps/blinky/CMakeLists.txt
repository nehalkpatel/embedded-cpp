cmake_minimum_required(VERSION 3.27)

add_executable(blinky blinky.hpp blinky.cpp)
target_compile_options(blinky PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(blinky PRIVATE board sys)
target_include_directories(blinky PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src/apps
  ${CMAKE_SOURCE_DIR}/src/libs/board
  ${CMAKE_SOURCE_DIR}/src/libs/mcu
  )