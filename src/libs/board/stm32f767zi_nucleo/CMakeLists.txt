add_library(nucleo_board nucleo_board.cpp)
target_sources(nucleo_board PUBLIC
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES nucleo_board.hpp pin_map.hpp)
target_link_libraries(nucleo_board
  PUBLIC board mcu arm_cm7_mcu
  PRIVATE project_options)

# The platform entry point (see CLAUDE.md for the contract). An OBJECT library
# so that main(), the vector table and the interrupt handlers are linked into
# each application directly: pulling them out of a static archive depends on
# link-order symbol resolution and breaks under --gc-sections and LTO, and a
# vector table nothing references is exactly what --gc-sections removes.
add_library(platform_entry OBJECT main.cpp startup.s syscalls.cpp)
target_link_libraries(platform_entry PRIVATE project_options app board nucleo_board arm_cm7_mcu)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/stm32f767zi.ld")

# INTERFACE rather than PRIVATE: platform_entry is an OBJECT library and never
# links anything itself. The link that needs these options is each application
# executable, which gets them through this target's usage requirements.
target_link_options(platform_entry INTERFACE
  "-T${LINKER_SCRIPT}"
  "LINKER:-Map=$<TARGET_PROPERTY:NAME>.map")

# INTERFACE_LINK_DEPENDS, not LINK_DEPENDS. The plain property is not
# transitive and would be attached to a target that never links, so editing the
# linker script would silently not relink the applications.
set_property(TARGET platform_entry APPEND
             PROPERTY INTERFACE_LINK_DEPENDS "${LINKER_SCRIPT}")
