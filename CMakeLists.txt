cmake_minimum_required(VERSION 3.27)

project(embedded-cpp-bsp VERSION 0.0.1 LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(COMMON_COMPILE_OPTIONS
  -Wall
  -Wextra
  -Werror
  -Wpedantic
  -Os
  -g
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(COMMON_COMPILE_OPTIONS ${COMMON_COMPILE_OPTIONS}
    -Wno-c++98-compat
    -Wno-exit-time-destructors
    -Wno-global-constructors
    -Wno-weak-vtables
    -fno-rtti
  )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(COMMON_COMPILE_OPTIONS ${COMMON_COMPILE_OPTIONS}
    -Wno-unknown-pragmas
    -fno-rtti
  )
endif()

include(FetchContent)

FetchContent_Declare(
  etl
  GIT_REPOSITORY https://github.com/ETLCPP/etl
  GIT_TAG 20.38.1
)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest
  GIT_TAG v1.14.0
)

FetchContent_Declare(
  stm32cubef7
  GIT_REPOSITORY https://github.com/STMicroelectronics/STM32CubeF7
  GIT_TAG v1.17.1
)

FetchContent_Declare(
  CmakeScripts
  GIT_REPOSITORY https://github.com/StaflSystems/cmake-scripts.git
  GIT_TAG main
)
FetchContent_GetProperties(CmakeScripts)

if(NOT cmakescripts_POPULATED)
  FetchContent_Populate(CmakeScripts)
  set(CMAKE_MODULE_PATH ${cmakescripts_SOURCE_DIR} ${CMAKE_MODULE_PATH})
endif()

# ZeroMQ itself, so we are not dependent on the system version
# Use this to get the latest version, once the issue is fixed. TODO: pin this version.
# FetchContent_Declare(zeromq GIT_REPOSITORY https://github.com/nehalkpatel/libzmq.git GIT_TAG fix/4629-compile-warning)
option(ZMQ_BUILD_TESTS OFF)
FetchContent_Declare(zeromq GIT_REPOSITORY https://github.com/zeromq/libzmq.git GIT_TAG b268effd886c9ea45f8a6f37f8d62b4bb045ad37)

# CPPZMQ header-only library; uses ZeroMQ
option(CPPZMQ_BUILD_TESTS OFF)
FetchContent_Declare(cppzmq GIT_REPOSITORY https://github.com/zeromq/cppzmq.git GIT_TAG v4.10.0)

# FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json GIT_TAG v3.11.2)

FetchContent_MakeAvailable(etl)

add_subdirectory(external)

include(tools)
# include(code-coverage)


if(CMAKE_PRESET STREQUAL "host")
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest)
  FetchContent_MakeAvailable(zeromq)
  FetchContent_MakeAvailable(cppzmq)
  FetchContent_MakeAvailable(json)
  include(CTest)
  enable_testing()
endif()

if(CMAKE_PRESET STREQUAL "arm-cm7")
  FetchContent_MakeAvailable(stm32cubef7)
endif()

clang_tidy("-header-filter=${CMAKE_CURRENT_SOURCE_DIR}/src/.*}")
add_subdirectory(src)
reset_clang_tidy()
clang_tidy("-header-filter=${CMAKE_CURRENT_SOURCE_DIR}/src/.*}")
add_subdirectory(test)
reset_clang_tidy()