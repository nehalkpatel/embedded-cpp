add_library(mcu INTERFACE pin.hpp uart.hpp i2c.hpp delay.hpp)
target_compile_options(mcu INTERFACE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(mcu INTERFACE error)

if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EMBEDDED_CPP_MCU}")
  message(FATAL_ERROR
    "MCU backend '${EMBEDDED_CPP_MCU}' is not implemented yet. "
    "Available: host. Hardware backends will be added later; see CLAUDE.md.")
endif()
add_subdirectory(${EMBEDDED_CPP_MCU})
