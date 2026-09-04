add_library(mcu INTERFACE)
target_sources(mcu INTERFACE
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES pin.hpp uart.hpp i2c.hpp delay.hpp)
target_link_libraries(mcu INTERFACE error)

# The list of backends is derived from the directories that actually exist, so
# the message cannot drift out of date as backends are added or removed.
if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EMBEDDED_CPP_MCU}")
  implemented_backends(available "${CMAKE_CURRENT_SOURCE_DIR}")
  message(FATAL_ERROR
    "MCU backend '${EMBEDDED_CPP_MCU}' is not implemented yet. "
    "Available: ${available}. See CLAUDE.md.")
endif()
add_subdirectory(${EMBEDDED_CPP_MCU})
