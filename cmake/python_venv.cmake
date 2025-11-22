function (configure_venv venv_name requirements_file)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(venv_path "${CMAKE_BINARY_DIR}/${venv_name}")

    add_custom_command(
        OUTPUT ${venv_path}
        COMMAND Python3::Interpreter -m venv ${venv_path}
        DEPENDS
            ${requirements_file}
        COMMENT "Creating virtual environment ${venv_name} at ${venv_path}"
    )

    add_custom_command(
        OUTPUT ${venv_path}/.venv_activated
        COMMAND /bin/bash -c 'source ${venv_path}/bin/activate'
        COMMAND touch ${venv_path}/.venv_activated
        DEPENDS
            ${venv_path}
        COMMENT "Activating virtual environment ${venv_name} with shell '$ENV{0}'"
    )

    set(ENV{VIRTUAL_ENV} ${venv_path})
    set(ENV{PATH} ${venv_path}/bin:$ENV{PATH})
    set(ENV{PYTHONPATH} ${venv_path}/lib/:$ENV{PYTHONPATH})

    add_custom_command(
        OUTPUT ${venv_path}/.requirements_installed
        COMMAND ${venv_path}/bin/python3 -m pip install -r ${CMAKE_CURRENT_SOURCE_DIR}/${requirements_file}
        COMMAND touch ${venv_path}/.requirements_installed
        DEPENDS
            ${requirements_file}
            ${venv_path}
            ${venv_path}/.venv_activated
        COMMENT "Installing requirements for ${venv_name}"
    )

    add_custom_command(
        OUTPUT ${venv_path}/.pip_list
        COMMAND ${venv_path}/bin/python3 -m pip list --format=columns > ${venv_path}/.pip_list
        DEPENDS
            ${venv_path}/.requirements_installed
        COMMENT "Listing installed packages for ${venv_name}"
    )

    add_custom_target(
        ${venv_name}
        ALL
        DEPENDS
            ${venv_path}/.requirements_installed
            ${venv_path}/.pip_list
    )

    set_property(TARGET ${venv_name} PROPERTY VIRTUAL_ENVIRONMENT TRUE)
    set_property(TARGET ${venv_name} PROPERTY VIRTUAL_ENVIRONMENT_DIRECTORY ${venv_path})
endfunction()
