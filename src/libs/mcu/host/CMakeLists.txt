cmake_minimum_required(VERSION 3.27)

add_library(host_mcu host_i2c.cpp host_pin.cpp delay.cpp)
target_compile_options(host_mcu PRIVATE ${COMMON_COMPILE_OPTIONS})

add_library(host_transport zmq_transport.cpp)
target_compile_options(host_transport PRIVATE ${COMMON_COMPILE_OPTIONS})

target_link_libraries(host_transport PRIVATE cppzmq)
target_link_libraries(host_mcu INTERFACE mcu PRIVATE host_transport nlohmann_json::nlohmann_json)

FetchContent_MakeAvailable(googletest)
add_library(GTest::GTest INTERFACE IMPORTED)
target_link_libraries(GTest::GTest INTERFACE gtest_main)

# Disable clang-tidy for test files due to dependency header issues
reset_clang_tidy()

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


include(GoogleTest)
gtest_discover_tests(test_host_transport)
gtest_discover_tests(test_messages)
gtest_discover_tests(test_dispatcher)

# Register test targets for code coverage
target_code_coverage(test_host_transport AUTO)
target_code_coverage(test_messages AUTO)
target_code_coverage(test_dispatcher AUTO)
