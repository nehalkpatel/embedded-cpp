# Host toolchain for clang/LLVM
# Compiler paths are set via CMakeUserPresets.json for machine-specific configuration
# This allows each developer to specify their preferred LLVM installation
# (e.g., Homebrew LLVM, system clang, or custom installation)

# If compilers are not set via presets, use default names
# CMake will search PATH to find them
if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER clang)
endif()

if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER clang++)
endif()

if(NOT DEFINED CMAKE_ASM_COMPILER)
    set(CMAKE_ASM_COMPILER clang)
endif()

if(NOT DEFINED CMAKE_LINKER)
    set(CMAKE_LINKER clang)
endif()

# Standard LLVM toolchain utilities
set(CMAKE_SIZE_UTIL     size)
set(CMAKE_OBJCOPY       objcopy)
set(CMAKE_OBJDUMP       objdump)
set(CMAKE_NM_UTIL       nm)
set(CMAKE_AR            ar)
set(CMAKE_RANLIB        ranlib)

