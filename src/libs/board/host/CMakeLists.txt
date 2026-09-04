add_library(host_board host_board.cpp)
target_sources(host_board PUBLIC
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES host_board.hpp)
target_link_libraries(host_board
  PUBLIC board mcu host_mcu
  PRIVATE project_options cppzmq nlohmann_json::nlohmann_json)

# The platform entry point, and the contract every board backend must satisfy:
# a target named `platform_entry` that provides main() and calls app::AppMain.
# The apps link it by name and know nothing else about the platform.
#
# An OBJECT library so main.o is linked into each app directly — pulling main()
# out of a static archive relies on link-time symbol-resolution order and
# breaks under --gc-sections/LTO. A hardware backend adds its startup code,
# vector table and syscalls here for the same reason.
add_library(platform_entry OBJECT main.cpp)
target_link_libraries(platform_entry PRIVATE project_options app board host_board)
