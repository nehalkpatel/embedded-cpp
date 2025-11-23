# Host toolchain for clang/LLVM
# Compiler path is set via CMakeUserPresets.json for machine-specific configuration
# This allows each developer to set their own path without modifying the project files
# (e.g., Homebrew LLVM, system clang, or custom installation)


if(DEFINED HOST_TOOLCHAIN_PATH)
    set(TOOLCHAIN_PREFIX "${HOST_TOOLCHAIN_PATH}/")
    set(CMAKE_FIND_ROOT_PATH ${HOST_TOOLCHAIN_PATH})
else()
    set(TOOLCHAIN_PREFIX "")
    message(STATUS "HOST_TOOLCHAIN_PATH not set, searching for clang in PATH")
endif()

if (NOT DEFINED HOST_TOOLCHAIN_VERSION)
  set(TOOLCHAIN_SUFFIX "-18") # Default to version 18 if not specified
else()
  set(TOOLCHAIN_SUFFIX "-${HOST_TOOLCHAIN_VERSION}")
endif()

# Standard LLVM toolchain utilities
set(CMAKE_C_COMPILER    ${TOOLCHAIN_PREFIX}clang${TOOLCHAIN_SUFFIX})
set(CMAKE_CXX_COMPILER  ${TOOLCHAIN_PREFIX}clang++${TOOLCHAIN_SUFFIX})
set(CMAKE_ASM_COMPILER  ${TOOLCHAIN_PREFIX}clang${TOOLCHAIN_SUFFIX})
set(CMAKE_SIZE_UTIL     ${TOOLCHAIN_PREFIX}llvm-size${TOOLCHAIN_SUFFIX})
set(CMAKE_OBJCOPY       ${TOOLCHAIN_PREFIX}llvm-objcopy${TOOLCHAIN_SUFFIX})
set(CMAKE_OBJDUMP       ${TOOLCHAIN_PREFIX}llvm-objdump${TOOLCHAIN_SUFFIX})
set(CMAKE_NM_UTIL       ${TOOLCHAIN_PREFIX}llvm-nm${TOOLCHAIN_SUFFIX})
set(CMAKE_AR            ${TOOLCHAIN_PREFIX}llvm-ar${TOOLCHAIN_SUFFIX})
set(CMAKE_RANLIB        ${TOOLCHAIN_PREFIX}llvm-ranlib${TOOLCHAIN_SUFFIX})

