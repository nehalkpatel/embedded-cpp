function (configure_venv venv_name requirements_file)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(venv_path "${CMAKE_BINARY_DIR}/${venv_name}")

    if(NOT EXISTS ${venv_path})
        execute_process(
            COMMAND ${Python3_EXECUTABLE} -m venv ${venv_path}
            RESULT_VARIABLE venv_creation_result
        )
        if(NOT venv_creation_result EQUAL 0)
            message(FATAL_ERROR "Failed to create virtual environment ${venv_name} at ${venv_path}")
        endif()
    endif()

    execute_process(
        COMMAND ${venv_path}/bin/pip install -q -r ${requirements_file}
        RESULT_VARIABLE pip_install_result
    )
    if(NOT pip_install_result EQUAL 0)
        message(FATAL_ERROR "Failed to install packages from ${requirements_file} into virtual environment ${venv_name}")
    endif()

    set(${venv_name}_PYTHON ${venv_path}/bin/python3 PARENT_SCOPE)

endfunction()
