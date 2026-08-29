add_library(host_mcu host_i2c.cpp host_pin.cpp host_uart.cpp delay.cpp)
target_sources(host_mcu PUBLIC
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES host_i2c.hpp host_pin.hpp host_uart.hpp
        dispatcher.hpp receiver.hpp
        host_emulator_messages.hpp emulator_message_json_encoder.hpp)
# nlohmann_json is PUBLIC because emulator_message_json_encoder.hpp, a public
# header, includes <nlohmann/json.hpp>.
target_link_libraries(host_mcu
  PUBLIC mcu nlohmann_json::nlohmann_json
  PRIVATE project_options host_transport)

add_library(host_transport zmq_transport.cpp endpoint_lock.cpp)
target_sources(host_transport PUBLIC
  FILE_SET HEADERS
  BASE_DIRS ${PROJECT_SOURCE_DIR}/src
  FILES zmq_transport.hpp transport.hpp endpoint_lock.hpp)
# cppzmq and logger are PUBLIC because zmq_transport.hpp includes their headers.
target_link_libraries(host_transport
  PUBLIC cppzmq logger error
  PRIVATE project_options)

# Coverage instrumentation for the libraries under test (the test executables
# themselves are handled by add_host_unit_test below).
if(CODE_COVERAGE)
  target_code_coverage(host_mcu)
  target_code_coverage(host_transport)
endif()

if(NOT BUILD_TESTING)
  return()
endif()

include(GoogleTest)

# Declares a host unit test in one place: builds it, registers it with CTest under
# the "unit" label, and wires up coverage. Trailing arguments are extra link
# libraries. Keeping all three in one function stops them drifting apart -- a test
# added without its LABELS or target_code_coverage line fails silently otherwise.
#
# `source` is explicit rather than derived from `name`: test_host_transport is
# built from test_zmq_transport.cpp.
function(add_host_unit_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE project_options GTest::gtest_main ${ARGN})
  # TIMEOUT is a backstop, not a budget: unit tests here run in well under a
  # second, and a startup bug in the transport used to wedge the constructor
  # forever rather than fail. A hung test should fail CI, not stall it.
  gtest_discover_tests(${name} PROPERTIES LABELS "unit" TIMEOUT 60)
  if(CODE_COVERAGE)
    # Exclusions are inherited from global add_code_coverage_all_targets()
    target_code_coverage(${name} AUTO ALL)
  endif()
endfunction()

add_host_unit_test(test_host_transport test_zmq_transport.cpp host_transport)
# Separate binary from test_host_transport on purpose: these tests bound a
# potential hang with a watchdog that hard-exits the process, which must not
# take unrelated tests down with it.
add_host_unit_test(test_host_transport_startup test_zmq_transport_startup.cpp
  host_transport)
add_host_unit_test(test_messages test_messages.cpp host_mcu)
add_host_unit_test(test_dispatcher test_dispatcher.cpp host_mcu)
add_host_unit_test(test_host_uart test_host_uart.cpp host_mcu host_transport)
add_host_unit_test(test_host_i2c test_host_i2c.cpp host_mcu host_transport)
