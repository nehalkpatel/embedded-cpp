# ARM GCC toolchain for embedded targets. Expects MCPU_FLAGS / VFP_FLAGS from
# the including per-core file (armgcc-cm4.cmake, armgcc-cm7.cmake).
#
# Toolchain path is set via ARM_TOOLCHAIN_PATH in CMakeUserPresets.json so each
# developer can point at their own installation without modifying the project.
#
# This file carries only what the tools need: target selection, codegen flags,
# and per-configuration optimization levels. Warning policy is a project
# decision and lives with the project's targets, not here.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(TARGET_TRIPLET "arm-none-eabi")

# If ARM_TOOLCHAIN_PATH is set, use it; otherwise rely on PATH
if(DEFINED ARM_TOOLCHAIN_PATH)
    set(TOOLCHAIN_PREFIX "${ARM_TOOLCHAIN_PATH}/${TARGET_TRIPLET}-")
    set(CMAKE_FIND_ROOT_PATH ${ARM_TOOLCHAIN_PATH})
else()
    set(TOOLCHAIN_PREFIX "${TARGET_TRIPLET}-")
    message(STATUS "ARM_TOOLCHAIN_PATH not set, searching for ${TARGET_TRIPLET}-gcc in PATH")
endif()

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}ranlib)

# Cross builds must never pick up host programs or libraries.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Compiler and linker flags
set(CMAKE_COMMON_FLAGS "${MCPU_FLAGS} ${VFP_FLAGS} -g3 -fstack-usage -ffunction-sections -fdata-sections -fno-strict-aliasing -fno-builtin -fno-common")

set(CMAKE_ASM_OPTIONS "-x assembler-with-cpp")
set(CMAKE_C_FLAGS_INIT "${CMAKE_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CMAKE_COMMON_FLAGS} ${CMAKE_ASM_OPTIONS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "--specs=nano.specs -Wl,--gc-sections,-print-memory-usage,--no-warn-rwx-segments")

set(CMAKE_C_FLAGS_DEBUG_INIT "-O0")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-O0")
set(CMAKE_ASM_FLAGS_DEBUG_INIT "")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG_INIT "")

set(CMAKE_C_FLAGS_RELEASE_INIT "-Os -flto")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Os -flto")
set(CMAKE_ASM_FLAGS_RELEASE_INIT "")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE_INIT "-flto")
