# Per-platform build artifacts for application executables.
#
# A host executable is the deliverable; a firmware image is not. The linker
# produces an ELF, and the board wants a raw binary (to copy onto the ST-LINK
# mass-storage volume) or Intel HEX (for most flashing tools).
#
# This lives here rather than in the board directory because the targets it
# decorates are the applications, and a board's CMakeLists cannot reach them --
# the board is added to the build after src/apps.

function(add_platform_artifacts target)
  if(NOT CMAKE_CROSSCOMPILING)
    return()  # A host build's executable is already what you run.
  endif()

  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}>
            $<TARGET_FILE_DIR:${target}>/${target}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${target}>
            $<TARGET_FILE_DIR:${target}>/${target}.hex
    # BYPRODUCTS resolves a narrower set of generator expressions than COMMAND
    # does -- TARGET_FILE_DIR is not among them -- so spell the output
    # directory out. It matches what TARGET_FILE_DIR expands to above, since
    # the applications use the project-wide runtime output directory.
    BYPRODUCTS
      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${target}.bin
      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/${target}.hex
    COMMENT "Creating ${target}.bin and ${target}.hex"
    VERBATIM)
endfunction()
