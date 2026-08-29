add_library(board INTERFACE board.hpp)
target_compile_options(board INTERFACE ${COMMON_COMPILE_OPTIONS})
set_target_properties(board PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(board INTERFACE mcu error)

if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${EMBEDDED_CPP_BOARD}")
  message(FATAL_ERROR
    "Board '${EMBEDDED_CPP_BOARD}' is not implemented yet. "
    "Available: host. Hardware boards will be added later; see CLAUDE.md.")
endif()
add_subdirectory(${EMBEDDED_CPP_BOARD})