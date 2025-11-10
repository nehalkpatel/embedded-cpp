# From https://github.com/ryanwinter/cmake-embedded-toolchains/blob/main/find_compiler.cmake
function(find_compiler compiler_path compiler_ext compiler_exe)
    message(STATUS "Looking for compiler executable: ${compiler_exe}")
    message(STATUS "CMAKE_TOOLCHAIN_PATH is set to: '$ENV{CMAKE_TOOLCHAIN_PATH}'")
    message(STATUS "CMAKE_PREFIX_PATH is set to: '$ENV{CMAKE_PREFIX_PATH}'")
    find_program(
        _compiler 
        NAME ${compiler_exe}
        PATHS ENV CMAKE_TOOLCHAIN_PATH PATH_SUFFIXES bin
    )

    if("${_compiler}" STREQUAL "${_compiler}-NOTFOUND")
        message(FATAL_ERROR "Compiler not found, you can specify search path with \"CMAKE_TOOLCHAIN_PATH\".")
    else()
        message(STATUS "Found compiler ${_compiler} in '${${compiler_path}}' with extension '${${compiler_ext}}'")
        get_filename_component(${compiler_path} ${_compiler} DIRECTORY CACHE)
        get_filename_component(${compiler_ext} ${_compiler} EXT CACHE)
    endif()
endfunction()
