# Host toolchain for clang/LLVM.
#
# Two ways to point at a specific installation, both via CMakeUserPresets.json
# (see CMakeUserPresets.json.example):
#   - set CMAKE_C_COMPILER / CMAKE_CXX_COMPILER cache variables directly, or
#   - set HOST_TOOLCHAIN_PATH (and optionally HOST_TOOLCHAIN_VERSION) and let
#     this file derive every tool from them.
# Explicitly set compilers always win: this file only fills in what the user
# has not chosen, so a preset's cache variables are never silently overridden.

if(DEFINED HOST_TOOLCHAIN_PATH)
    set(TOOLCHAIN_PREFIX "${HOST_TOOLCHAIN_PATH}/")
    set(CMAKE_FIND_ROOT_PATH ${HOST_TOOLCHAIN_PATH})
else()
    set(TOOLCHAIN_PREFIX "")
endif()

if(NOT DEFINED HOST_TOOLCHAIN_VERSION)
  set(TOOLCHAIN_SUFFIX "-18") # Default to version 18 if not specified
else()
  set(TOOLCHAIN_SUFFIX "-${HOST_TOOLCHAIN_VERSION}")
endif()

if(NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER    ${TOOLCHAIN_PREFIX}clang${TOOLCHAIN_SUFFIX})
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER  ${TOOLCHAIN_PREFIX}clang++${TOOLCHAIN_SUFFIX})
endif()
if(NOT DEFINED CMAKE_ASM_COMPILER)
  set(CMAKE_ASM_COMPILER  ${TOOLCHAIN_PREFIX}clang${TOOLCHAIN_SUFFIX})
endif()

set(CMAKE_OBJCOPY       ${TOOLCHAIN_PREFIX}llvm-objcopy${TOOLCHAIN_SUFFIX})
set(CMAKE_OBJDUMP       ${TOOLCHAIN_PREFIX}llvm-objdump${TOOLCHAIN_SUFFIX})
set(CMAKE_AR            ${TOOLCHAIN_PREFIX}llvm-ar${TOOLCHAIN_SUFFIX})
set(CMAKE_RANLIB        ${TOOLCHAIN_PREFIX}llvm-ranlib${TOOLCHAIN_SUFFIX})

# Consumed by the code-coverage module (StableCoder cmake-scripts)
set(LLVM_COV_PATH       ${TOOLCHAIN_PREFIX}llvm-cov${TOOLCHAIN_SUFFIX})
set(LLVM_PROFDATA_PATH  ${TOOLCHAIN_PREFIX}llvm-profdata${TOOLCHAIN_SUFFIX})
