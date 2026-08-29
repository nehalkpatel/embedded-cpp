add_library(board INTERFACE)
target_sources(board INTERFACE
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES board.hpp)
target_link_libraries(board INTERFACE mcu error)

if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EMBEDDED_CPP_BOARD}")
  message(FATAL_ERROR
    "Board '${EMBEDDED_CPP_BOARD}' is not implemented yet. "
    "Available: host. Hardware boards will be added later; see CLAUDE.md.")
endif()
add_subdirectory(${EMBEDDED_CPP_BOARD})
