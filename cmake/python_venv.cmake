# Python virtual environment management using uv
#
# Usage:
#   configure_venv(venv_name project_dir)
#
# Syncs `project_dir` (and its dev dependency group) into a venv under the CMake
# binary directory, using the project's uv.lock so builds are reproducible.
#
# Sets ${venv_name}_PYTHON to the path of the Python interpreter in the venv.

function(configure_venv venv_name project_dir)
    set(venv_path "${CMAKE_BINARY_DIR}/${venv_name}")

    # Find uv - prefer system installation, fall back to common locations
    find_program(UV_EXECUTABLE uv
        HINTS
            $ENV{HOME}/.cargo/bin
            $ENV{HOME}/.local/bin
            /usr/local/bin
    )

    if(NOT UV_EXECUTABLE)
        message(FATAL_ERROR
            "uv not found. Install it with: curl -LsSf https://astral.sh/uv/install.sh | sh"
        )
    endif()

    message(STATUS "Using uv: ${UV_EXECUTABLE}")
    message(STATUS "Syncing ${venv_name} from ${project_dir}/uv.lock")

    # `uv sync --frozen` installs exactly what uv.lock pins and fails rather than
    # silently re-resolving if the lock is stale. It is a no-op once the venv
    # matches the lock, so repeat configures stay cheap.
    #
    # UV_PROJECT_ENVIRONMENT redirects the venv out of the source tree and into
    # the build directory. The interpreter comes from .python-version.
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env
                "UV_PROJECT_ENVIRONMENT=${venv_path}"
                ${UV_EXECUTABLE} sync --frozen
        WORKING_DIRECTORY ${project_dir}
        RESULT_VARIABLE sync_result
    )
    if(NOT sync_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to sync ${venv_name} from ${project_dir}/uv.lock. "
            "If dependencies changed, refresh the lock with: uv lock"
        )
    endif()

    # Re-run CMake if the dependency manifests change
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${project_dir}/pyproject.toml"
        "${project_dir}/uv.lock"
    )

    # Export the Python path
    set(${venv_name}_PYTHON ${venv_path}/bin/python3 PARENT_SCOPE)
endfunction()
