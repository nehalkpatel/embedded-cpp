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

# The hardware backend has no tests of its own: cross builds run no CTest and
# have no googletest. But the arm_cm7 public headers deliberately name no vendor
# type, so the contract they encode compiles -- and can be asserted -- on the
# host, whichever backend is selected. add_host_unit_test comes from the host
# backend's CMakeLists, and CMake functions are global once defined.
#
# include(GoogleTest) again here rather than relying on the host backend's: it
# sets the discovery script path as a directory-scoped variable, so without it
# gtest_discover_tests silently registers a _NOT_BUILT placeholder instead of
# the tests.
if(BUILD_TESTING AND EMBEDDED_CPP_MCU STREQUAL "host")
  include(GoogleTest)
  add_host_unit_test(test_peripheral_contract
    arm_cm7/test_peripheral_contract.cpp mcu)
endif()
