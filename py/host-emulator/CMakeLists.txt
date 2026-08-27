include("${PROJECT_SOURCE_DIR}/cmake/python_venv.cmake")

configure_venv(host_emulator_venv ${CMAKE_CURRENT_SOURCE_DIR})

add_test(
    NAME host_emulator_test
    COMMAND ${host_emulator_venv_PYTHON} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}
            --blinky=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/blinky
            --uart-echo=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/uart_echo
            --i2c-demo=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/i2c_demo
    WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
)

# No DEPENDS here: it names tests, not targets, and the executables under test are
# built before ctest runs. The venv is created at configure time by configure_venv()
# above, so it is not a test either and cannot be a fixture.
set_tests_properties(host_emulator_test PROPERTIES
    TIMEOUT 300
    LABELS "integration"
)

