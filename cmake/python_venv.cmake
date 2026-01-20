# Python virtual environment management using uv
#
# Usage:
#   configure_venv(venv_name project_dir)
#
# This creates a virtual environment using uv and installs the project
# in editable mode with dev dependencies.
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

    # Create venv and install project with dev dependencies
    if(NOT EXISTS "${venv_path}/bin/python3")
        message(STATUS "Creating virtual environment: ${venv_name}")
        execute_process(
            COMMAND ${UV_EXECUTABLE} venv ${venv_path} --python 3.11
            RESULT_VARIABLE venv_result
        )
        if(NOT venv_result EQUAL 0)
            message(FATAL_ERROR "Failed to create virtual environment: ${venv_name}")
        endif()
    endif()

    # Install project in editable mode with dev dependencies using uv pip
    message(STATUS "Installing ${venv_name} dependencies with uv")
    execute_process(
        COMMAND ${UV_EXECUTABLE} pip install -e ${project_dir}[dev] --python ${venv_path}/bin/python3
        RESULT_VARIABLE install_result
    )
    if(NOT install_result EQUAL 0)
        message(FATAL_ERROR "Failed to install project into ${venv_name}")
    endif()

    # Export the Python path
    set(${venv_name}_PYTHON ${venv_path}/bin/python3 PARENT_SCOPE)
endfunction()
