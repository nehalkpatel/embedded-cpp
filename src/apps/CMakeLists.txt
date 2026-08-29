add_library(app INTERFACE)
target_sources(app INTERFACE
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES app.hpp)
target_link_libraries(app INTERFACE board error)

add_subdirectory(blinky)
add_subdirectory(uart_echo)
add_subdirectory(i2c_demo)
