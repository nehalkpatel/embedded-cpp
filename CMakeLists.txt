cmake_minimum_required(VERSION 3.27)

project(embedded-cpp-bsp VERSION 0.0.1 LANGUAGES CXX C ASM)

# ---------------------------------------------------------------------------
# Project-wide setup
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Defines the BUILD_TESTING option (ON by default) and calls enable_testing().
# Testing is enabled here, unconditionally and early, so no add_test() call in
# a subdirectory can be silently discarded.
include(CTest)

# Platform selection. The presets set these; a plain `cmake -B build` gets the
# host defaults, so the project configures without any preset at all.
set(EMBEDDED_CPP_MCU "host" CACHE STRING
    "MCU backend to build (selects src/libs/mcu/<value>)")
set_property(CACHE EMBEDDED_CPP_MCU PROPERTY STRINGS host arm_cm4 arm_cm7)
set(EMBEDDED_CPP_BOARD "host" CACHE STRING
    "Board implementation to build (selects src/libs/board/<value>)")
set_property(CACHE EMBEDDED_CPP_BOARD PROPERTY STRINGS host stm32f3_discovery)

# One bin/ directory per build tree (with per-config subdirectories under the
# multi-config generator). A build-layout decision, so it lives here rather
# than in the presets.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

# Keep a compile_commands.json symlink at the repo root so clangd and IDEs find
# the database of whichever build tree was configured last. The link is
# regenerated on every configure and is gitignored.
file(CREATE_LINK "${PROJECT_BINARY_DIR}/compile_commands.json"
     "${PROJECT_SOURCE_DIR}/compile_commands.json" SYMBOLIC)

# Common usage requirements, carried by INTERFACE targets so each consumer
# states its dependency explicitly instead of inheriting directory state.
#
# project_warnings: warnings only. Optimization and debug-info flags are
# per-configuration and must not be set here: these options are appended after
# the per-config flags, and the last -O on the command line wins, so a global
# -Os would silently override Release's -O3. The ARM toolchain sets its own
# CMAKE_*_FLAGS_{DEBUG,RELEASE}_INIT (cmake/toolchain/armgcc.cmake); the host
# build uses CMake's defaults.
add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
  -Wall
  -Wextra
  -Werror
  -Wpedantic
  $<$<CXX_COMPILER_ID:Clang>:-Wno-c++98-compat;-Wno-exit-time-destructors;-Wno-global-constructors;-Wno-weak-vtables>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-unknown-pragmas>
)

# project_options: everything a target of this project compiles with beyond
# warnings — the src/ include root (headers are included by their project-
# relative path, e.g. "libs/mcu/pin.hpp") and the no-RTTI policy.
add_library(project_options INTERFACE)
target_include_directories(project_options INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>)
target_compile_options(project_options INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
target_link_libraries(project_options INTERFACE project_warnings)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

include(FetchContent)

# GIT_TAG pins the commit hash (with the tag name alongside for the reader):
# tags can move, hashes cannot.

# Reusable CMake modules: clang-tidy integration (tools) and coverage
FetchContent_Declare(
  CmakeScripts
  GIT_REPOSITORY https://github.com/StableCoder/cmake-scripts.git
  GIT_TAG 5b6c6efaeaab749001b1a1323f46e0ba7cf1c01f # 25.08
)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest
  GIT_TAG f8d7d77c06936315286eb55f8de22cd23c188571 # v1.14.0
)

# cppzmq is a header-only C++ binding; the libzmq it binds is expected to be
# installed on the system (libzmq3-dev).
FetchContent_Declare(
  cppzmq
  GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
  GIT_TAG c94c20743ed7d4aa37835a5c46567ab0790d4acc # v4.10.0
)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json
  GIT_TAG 0ca0fe433eb70cea0d5761079c0c5b47b736565b # v3.11.2
)

FetchContent_MakeAvailable(CmakeScripts)
list(PREPEND CMAKE_MODULE_PATH "${cmakescripts_SOURCE_DIR}")

# ZeroMQ, JSON, and googletest serve the host-emulation platform; a hardware
# build has no use for them.
if(EMBEDDED_CPP_MCU STREQUAL "host")
  set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(cppzmq json)

  if(BUILD_TESTING)
    # For Windows: prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()
endif()

# ---------------------------------------------------------------------------
# Tooling: clang-tidy, coverage, formatting
# ---------------------------------------------------------------------------

include(tools)
include(code-coverage)

# Configure global code coverage settings
if(CODE_COVERAGE)
  # Create 'ccov-all' target and set global exclusions
  add_code_coverage_all_targets(
    EXCLUDE "test_*.cpp" "*/test/*" "*/_deps/*" "*/googletest/*" "*/gtest/*"
    LLVM_EXCLUDE ".*/test_.*\\.cpp" ".*/test/.*" ".*/_deps/.*" ".*/googletest/.*" ".*/gtest/.*"
    LCOV_EXCLUDE "*/test_*.cpp" "*/test/*" "*/_deps/*" "*/googletest/*" "*/gtest/*"
  )
endif()

# `format` / `format-check` targets, and the pre-commit hook. Included for every
# configuration: formatting is not host-specific, and a cross-compiling
# developer should get the same guard rails.
include("${PROJECT_SOURCE_DIR}/cmake/format.cmake")

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

clang_tidy("-header-filter=${PROJECT_SOURCE_DIR}/src/.*")
add_subdirectory(src)
reset_clang_tidy()

# The Python emulator and its integration tests exist to test the host build.
if(EMBEDDED_CPP_MCU STREQUAL "host")
  add_subdirectory(py/host-emulator)   # No clang-tidy for python
endif()
