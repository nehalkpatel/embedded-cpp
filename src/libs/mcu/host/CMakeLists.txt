cmake_minimum_required(VERSION 3.27)

add_library(host_mcu host_i2c.cpp host_pin.cpp delay.cpp)

add_library(host_transport zmq_transport.cpp)
target_link_libraries(host_transport PRIVATE cppzmq)

target_compile_options(host_mcu PRIVATE ${COMMON_COMPILE_OPTIONS})
set_target_properties(host_mcu PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(host_mcu INTERFACE mcu PRIVATE host_transport nlohmann_json::nlohmann_json)

FetchContent_MakeAvailable(googletest)
add_library(GTest::GTest INTERFACE IMPORTED)
target_link_libraries(GTest::GTest INTERFACE gtest_main)

add_executable(test_host_transport test_zmq_transport.cpp)

target_link_libraries(test_host_transport
 PRIVATE
  GTest::GTest
  host_transport
  cppzmq # needed because zmq_transport.hpp includes zmq.hpp
  )


include(GoogleTest)
gtest_discover_tests(test_host_transport)
