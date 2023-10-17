cmake_minimum_required(VERSION 3.27)

include(FetchContent)
option(ZMQ_BUILD_TESTS OFF)
FetchContent_Declare(zeromq GIT_REPOSITORY https://github.com/zeromq/libzmq.git GIT_TAG master)

# FetchContent_Declare( zeromq GIT_REPOSITORY https://github.com/nehalkpatel/libzmq.git GIT_TAG nehal/fix-unused-warnings )
# set(OLD_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-unused-parameter")
FetchContent_MakeAvailable(zeromq)

# target_compile_options(libzmq PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
# set(CMAKE_CXX_FLAGS ${OLD_CMAKE_CXX_FLAGS})
option(CPPZMQ_BUILD_TESTS OFF)
FetchContent_Declare(cppzmq GIT_REPOSITORY https://github.com/zeromq/cppzmq.git GIT_TAG v4.10.0)
FetchContent_MakeAvailable(cppzmq)
target_compile_options(cppzmq INTERFACE
  -Wno-unused-parameter
  -Wno-unsafe-buffer-usage
  -Wno-reserved-macro-identifier
  -Wno-padded
  -Wno-deprecated-literal-operator
  -Wno-missing-noreturn
)

FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json GIT_TAG v3.11.2)
# FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)

FetchContent_MakeAvailable(json)

# FetchContent_Declare(zmqpp GIT_REPOSITORY https://github.com/zeromq/zmqpp.git GIT_TAG        master )
# FetchContent_MakeAvailable(zmqpp)

add_library(host_board host_board.hpp host_board.cpp)
target_compile_options(host_board PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(host_board  PRIVATE board mcu host_mcu cppzmq nlohmann_json::nlohmann_json)
target_include_directories(host_board INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_library(sys main.cpp)
target_compile_options(sys PRIVATE ${COMMON_COMPILE_OPTIONS})
target_link_libraries(sys PRIVATE app board mcu host_board host_mcu cppzmq)
target_include_directories(sys INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)
