# Platform-backend discovery.
#
# The MCU and board layers each select an implementation by directory name
# (EMBEDDED_CPP_MCU / EMBEDDED_CPP_BOARD). When the requested name has no
# directory, the error message should say what *is* available -- and say it
# correctly. A hand-written list drifts: it named only "host" long after
# stm32f3_discovery became a visible preset.

# Set <out_var> to a comma-separated, sorted list of the implementation
# directories under <dir>. A directory counts as an implementation only if it
# has a CMakeLists.txt, so a stray or half-deleted directory is not advertised
# as a working backend.
function(implemented_backends out_var dir)
  file(GLOB entries LIST_DIRECTORIES true "${dir}/*")
  set(found "")
  foreach(entry IN LISTS entries)
    if(IS_DIRECTORY "${entry}" AND EXISTS "${entry}/CMakeLists.txt")
      cmake_path(GET entry FILENAME name)
      list(APPEND found "${name}")
    endif()
  endforeach()
  list(SORT found)
  list(JOIN found ", " joined)
  set(${out_var} "${joined}" PARENT_SCOPE)
endfunction()
