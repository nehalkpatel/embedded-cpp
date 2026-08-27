# Formatting targets, and installation of the pre-commit hook.
#
# Both delegate to tools/format.sh rather than reimplementing the commands, so
# `cmake --build ... --target format-check`, the pre-commit hook, and CI are
# guaranteed to agree. Anything that duplicates the rules eventually disagrees
# with CI, which is the failure mode this file exists to prevent.

set(FORMAT_SCRIPT "${CMAKE_SOURCE_DIR}/tools/format.sh")

if(NOT EXISTS "${FORMAT_SCRIPT}")
  message(WARNING "tools/format.sh not found; format targets unavailable")
  return()
endif()

add_custom_target(format
  COMMAND "${FORMAT_SCRIPT}" --fix
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Reformatting C++ and Python sources in place"
  USES_TERMINAL
  VERBATIM
)

add_custom_target(format-check
  COMMAND "${FORMAT_SCRIPT}" --check
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Checking formatting (same checks as CI)"
  USES_TERMINAL
  VERBATIM
)

# Install the pre-commit hook by pointing core.hooksPath at the version
# controlled .githooks directory.
#
# This writes to the developer's local git config, which is why it is announced
# rather than done silently, and why it is opt-out. It is skipped outside a git
# work tree so that tarball builds and CI checkouts are unaffected.
option(INSTALL_GIT_HOOKS "Point core.hooksPath at .githooks during configure" ON)

if(INSTALL_GIT_HOOKS AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  find_program(GIT_EXECUTABLE git)
  if(GIT_EXECUTABLE)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" config --get core.hooksPath
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE current_hooks_path
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(NOT current_hooks_path STREQUAL ".githooks")
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" config core.hooksPath .githooks
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE hooks_result
        ERROR_QUIET
      )
      if(hooks_result EQUAL 0)
        message(STATUS "Set git core.hooksPath to .githooks (pre-commit format check)")
        message(STATUS "  Opt out with -DINSTALL_GIT_HOOKS=OFF; bypass once with git commit --no-verify")
      endif()
    endif()
  endif()
endif()
