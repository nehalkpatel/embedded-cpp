include("${PROJECT_SOURCE_DIR}/cmake/python_venv.cmake")

configure_venv(host_emulator_venv ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt)

add_test(
    NAME host_emulator_test
    COMMAND ${host_emulator_venv_PYTHON} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}
            --blinky=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/blinky 
    WORKING_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}
)

set_tests_properties(host_emulator_test PROPERTIES DEPENDS blinky)
set_tests_properties(host_emulator_test PROPERTIES DEPENDS host_emulator_venv)
set_tests_properties(host_emulator_test PROPERTIES FIXTURES_SETUP host_emulator_venv)

