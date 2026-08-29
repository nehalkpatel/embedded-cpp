add_library(host_board host_board.cpp)
target_sources(host_board PUBLIC
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES host_board.hpp)
target_link_libraries(host_board
  PUBLIC board mcu host_mcu
  PRIVATE project_options cppzmq nlohmann_json::nlohmann_json)

# The platform entry point. An OBJECT library so main.o is linked into each
# app directly — pulling main() out of a static archive relies on link-time
# symbol-resolution order and breaks under --gc-sections/LTO.
add_library(sys OBJECT main.cpp)
target_link_libraries(sys PRIVATE project_options app board host_board)
