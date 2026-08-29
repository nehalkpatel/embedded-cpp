# The Python integration tests run out of a uv-managed venv in the build tree.
# Creating the venv is itself a CTest fixture: `uv sync --frozen` installs
# exactly what uv.lock pins (and is a cheap no-op once the venv matches), so
# configure stays fast and offline and the venv refreshes exactly when the
# tests need it.

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

set(venv_path "${PROJECT_BINARY_DIR}/host_emulator_venv")

add_test(
    NAME host_emulator_venv
    COMMAND ${CMAKE_COMMAND} -E env
            "UV_PROJECT_ENVIRONMENT=${venv_path}"
            ${UV_EXECUTABLE} sync --frozen
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(host_emulator_venv PROPERTIES
    FIXTURES_SETUP emulator_venv
    TIMEOUT 300
    LABELS "integration"
)

# $<TARGET_FILE:...> resolves the real executable path for whatever
# configuration and output layout is in effect — no hand-built paths.
add_test(
    NAME host_emulator_test
    COMMAND ${venv_path}/bin/python3 -m pytest ${CMAKE_CURRENT_SOURCE_DIR}
            --blinky=$<TARGET_FILE:blinky>
            --uart-echo=$<TARGET_FILE:uart_echo>
            --i2c-demo=$<TARGET_FILE:i2c_demo>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(host_emulator_test PROPERTIES
    FIXTURES_REQUIRED emulator_venv
    TIMEOUT 300
    LABELS "integration"
)
