cmake_minimum_required(VERSION 3.27)

add_library(host_mcu host_i2c.cpp host_pin.cpp host_uart.cpp delay.cpp)
target_compile_options(host_mcu PRIVATE ${COMMON_COMPILE_OPTIONS})

add_library(host_transport zmq_transport.cpp)
target_compile_options(host_transport PRIVATE ${COMMON_COMPILE_OPTIONS})

target_link_libraries(host_transport PRIVATE cppzmq logger)
target_link_libraries(host_mcu INTERFACE mcu PRIVATE host_transport nlohmann_json::nlohmann_json)

FetchContent_MakeAvailable(googletest)
add_library(GTest::GTest INTERFACE IMPORTED)
target_link_libraries(GTest::GTest INTERFACE gtest_main)

add_executable(test_host_transport test_zmq_transport.cpp)
target_compile_options(test_host_transport PRIVATE ${COMMON_COMPILE_OPTIONS})

target_link_libraries(test_host_transport
 PRIVATE
  GTest::GTest
  host_transport
  cppzmq # needed because zmq_transport.hpp includes zmq.hpp
  )

add_executable(test_messages test_messages.cpp)
target_compile_options(test_messages PRIVATE ${COMMON_COMPILE_OPTIONS})

target_link_libraries(test_messages
 PRIVATE
  GTest::GTest
  nlohmann_json::nlohmann_json
  )

add_executable(test_dispatcher test_dispatcher.cpp)
target_compile_options(test_dispatcher PRIVATE ${COMMON_COMPILE_OPTIONS})

target_link_libraries(test_dispatcher
 PRIVATE
  GTest::GTest
  )

add_executable(test_host_uart test_host_uart.cpp)
target_compile_options(test_host_uart PRIVATE ${COMMON_COMPILE_OPTIONS})

target_link_libraries(test_host_uart
 PRIVATE
  GTest::GTest
  host_mcu
  host_transport
  nlohmann_json::nlohmann_json
  cppzmq
  )

add_executable(test_host_i2c test_host_i2c.cpp)
target_compile_options(test_host_i2c PRIVATE ${COMMON_COMPILE_OPTIONS})

target_link_libraries(test_host_i2c
 PRIVATE
  GTest::GTest
  host_mcu
  host_transport
  nlohmann_json::nlohmann_json
  cppzmq
  )


include(GoogleTest)
gtest_discover_tests(test_host_transport)
gtest_discover_tests(test_messages)
gtest_discover_tests(test_dispatcher)
gtest_discover_tests(test_host_uart)
gtest_discover_tests(test_host_i2c)

# Code coverage configuration
if(CODE_COVERAGE)
  # Add coverage instrumentation to the libraries being tested
  target_code_coverage(host_mcu)
  target_code_coverage(host_transport)

  # Add coverage targets for each test executable
  # Exclusions are inherited from global add_code_coverage_all_targets()
  target_code_coverage(test_host_transport AUTO ALL)
  target_code_coverage(test_messages AUTO ALL)
  target_code_coverage(test_dispatcher AUTO ALL)
  target_code_coverage(test_host_uart AUTO ALL)
  target_code_coverage(test_host_i2c AUTO ALL)
endif()
