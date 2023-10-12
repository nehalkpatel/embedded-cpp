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
    # -Weverything
    -Wno-c++98-compat
    -Wno-exit-time-destructors
    -Wno-global-constructors
    -Wno-weak-vtables
  )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(COMMON_COMPILE_OPTIONS ${COMMON_COMPILE_OPTIONS}
    -Wno-unknown-pragmas
  )
endif()

add_subdirectory(src)
add_subdirectory(external)

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

FetchContent_MakeAvailable(etl)

if(CMAKE_PRESET STREQUAL "host")
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(CMAKE_CXX_CLANG_TIDY "clang-tidy")
  FetchContent_MakeAvailable(googletest)
endif()

if(CMAKE_PRESET STREQUAL "arm-cm7")
  FetchContent_MakeAvailable(stm32cubef7)
endif()

# process test/CMakeLists.txt
add_subdirectory(test)