*** Settings ***
Documentation       Test that CAF prints a helpful error message when a type ID block
...                 is missing from initialization.

Library             OperatingSystem
Library             Process


*** Variables ***
${BINARY_PATH}          /path/to/the/test/binary


*** Keywords ***
Run Type ID Test
    ${result}=    Run Process    ${BINARY_PATH}
    ...    stderr=STDOUT
    RETURN    ${result}


*** Test Cases ***
Missing Type ID Block Prints Helpful Error Message
    [Documentation]    When a type ID block is not initialized but its types are
    ...    used, CAF should print a helpful error message about the missing
    ...    type registration.
    ${result}=    Run Type ID Test
    Should Not Be Equal As Integers    ${result.rc}    0
    ...    msg=Process should have crashed with non-zero exit code
    Should Contain    ${result.stdout}    found no meta object for type ID
    ...    msg=Error message should mention missing meta object

