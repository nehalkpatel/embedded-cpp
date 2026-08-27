add_library(host_mcu host_i2c.cpp host_pin.cpp host_uart.cpp delay.cpp)
target_compile_options(host_mcu PRIVATE ${COMMON_COMPILE_OPTIONS})

add_library(host_transport zmq_transport.cpp)
target_compile_options(host_transport PRIVATE ${COMMON_COMPILE_OPTIONS})

target_link_libraries(host_transport PRIVATE cppzmq logger)
target_link_libraries(host_mcu INTERFACE mcu PRIVATE host_transport nlohmann_json::nlohmann_json)

FetchContent_MakeAvailable(googletest)
add_library(GTest::GTest INTERFACE IMPORTED)
target_link_libraries(GTest::GTest INTERFACE gtest_main)

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
  target_compile_options(${name} PRIVATE ${COMMON_COMPILE_OPTIONS})
  target_link_libraries(${name} PRIVATE GTest::GTest ${ARGN})
  gtest_discover_tests(${name} PROPERTIES LABELS "unit")
  if(CODE_COVERAGE)
    # Exclusions are inherited from global add_code_coverage_all_targets()
    target_code_coverage(${name} AUTO ALL)
  endif()
endfunction()

# cppzmq is needed because zmq_transport.hpp includes zmq.hpp
add_host_unit_test(test_host_transport test_zmq_transport.cpp host_transport cppzmq)
add_host_unit_test(test_messages test_messages.cpp nlohmann_json::nlohmann_json)
add_host_unit_test(test_dispatcher test_dispatcher.cpp)
add_host_unit_test(test_host_uart test_host_uart.cpp
  host_mcu host_transport nlohmann_json::nlohmann_json cppzmq)
add_host_unit_test(test_host_i2c test_host_i2c.cpp
  host_mcu host_transport nlohmann_json::nlohmann_json cppzmq)

# Coverage instrumentation for the libraries under test (the test executables
# themselves are handled by add_host_unit_test above).
if(CODE_COVERAGE)
  target_code_coverage(host_mcu)
  target_code_coverage(host_transport)
endif()
