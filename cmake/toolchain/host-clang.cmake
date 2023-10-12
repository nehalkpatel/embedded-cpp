include("${CMAKE_CURRENT_LIST_DIR}/../find_compiler.cmake")


# Find the compiler path
find_compiler(HOST_COMPILER_PATH HOST_COMPILER_EXT "${TARGET_TRIPLET}clang")

# where is the target environment located
set(CMAKE_FIND_ROOT_PATH ${HOST_COMPILER_PATH})

set(CMAKE_C_COMPILER    ${HOST_COMPILER_PATH}/clang)
set(CMAKE_CXX_COMPILER  ${HOST_COMPILER_PATH}/clang++)
set(CMAKE_ASM_COMPILER  ${HOST_COMPILER_PATH}/clang)
set(CMAKE_LINKER        ${HOST_COMPILER_PATH}/clang)
set(CMAKE_SIZE_UTIL     ${HOST_COMPILER_PATH}/llvm-size)
set(CMAKE_OBJCOPY       ${HOST_COMPILER_PATH}/llvm-objcopy)
set(CMAKE_OBJDUMP       ${HOST_COMPILER_PATH}/llvm-objdump)
set(CMAKE_NM_UTIL       ${HOST_COMPILER_PATH}/llvm-nm)
set(CMAKE_AR            ${HOST_COMPILER_PATH}/llvm-ar)
set(CMAKE_RANLIB        ${HOST_COMPILER_PATH}/llvm-ranlib)