*** Settings ***
Documentation     A test suite for --config-file.
Library           OperatingSystem
Library           Process


*** Variables ***
${BINARY_PATH}    /path/to/the/test/binary


*** Test Cases ***
Test --config-file argument
    [Documentation]  Binary with a --config-file option tries to open the provided file.
    [Tags]    Config
    Run Process  ${BINARY_PATH}  --config-file\=test-me.cfg  stderr=output
    ${error_output}=  Get File  output
    Should Contain  ${error_output}  cannot_open_file("test-me.cfg")
    Run Process  ${BINARY_PATH}  --config-file  test-me.cfg  stderr=output
    ${error_output}=  Get File  output
    Should Contain  ${error_output}  cannot_open_file("test-me.cfg")
