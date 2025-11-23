# ARM GCC Toolchain file for embedded targets
# Toolchain path is set via ARM_TOOLCHAIN_PATH environment in CMakeUserPresets.json
# This allows each developer to set their own path without modifying the project files

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_CROSSCOMPILING 1)
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
set(CMAKE_LINKER       ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_SIZE_UTIL    ${TOOLCHAIN_PREFIX}size)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump)
set(CMAKE_NM_UTIL      ${TOOLCHAIN_PREFIX}nm)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}ranlib)


set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Compiler and linker flags
set(CMAKE_COMMON_FLAGS "${MCPU_FLAGS} ${VFP_FLAGS} -g3 -fstack-usage -ffunction-sections -fdata-sections -fno-strict-aliasing -fno-builtin -fno-common -Wall -Wshadow -Wdouble-promotion -Werror -Wundef -Wformat=2 -Wno-unused-parameter")

SET(CMAKE_ASM_OPTIONS "-x assembler-with-cpp")
set(CMAKE_C_FLAGS_INIT "${CMAKE_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CMAKE_COMMON_FLAGS} ${CMAKE_ASM_OPTIONS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${LD_FLAGS} --specs=nano.specs -Wl,--gc-sections,-print-memory-usage,--no-warn-rwx-segments")

set(CMAKE_C_FLAGS_DEBUG_INIT "-O0")
set(CMAKE_CXX_ASM_FLAGS_DEBUG_INIT "-O0")
set(CMAKE_ASM_FLAGS_DEBUG_INIT "")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG_INIT "")

set(CMAKE_C_FLAGS_RELEASE_INIT "-Os -flto")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Os -flto")
set(CMAKE_ASM_FLAGS_RELEASE_INIT "")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE_INIT "-flto")

# // Make sure the executable comes after the shared libaries, for symbol resolution
# set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> -o <TARGET> <LINK_LIBRARIES> <OBJECTS> ")
