add_library(mcu INTERFACE)
target_sources(mcu INTERFACE
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES pin.hpp uart.hpp i2c.hpp delay.hpp)
target_link_libraries(mcu INTERFACE error)

if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EMBEDDED_CPP_MCU}")
  message(FATAL_ERROR
    "MCU backend '${EMBEDDED_CPP_MCU}' is not implemented yet. "
    "Available: host. Hardware backends will be added later; see CLAUDE.md.")
endif()
add_subdirectory(${EMBEDDED_CPP_MCU})
