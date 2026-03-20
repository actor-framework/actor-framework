*** Settings ***
Documentation     A test suite for libcaf_test/caf-test-test
Library           Process
Library           String
Library           OperatingSystem

*** Variables ***
${BINARY_PATH}           /path/to/the/test

*** Test Cases ***
Test Outline Template
    ${handle}=  Start Process    ${BINARY_PATH}
    ...  -s     outline
    ...  -t     eating cucumbers
    ...  -v     debug
    ...  stdout=test.out
    ...  stderr=test.err

    Wait For Process    ${handle}    timeout=5s

    ${outlines}=    Find String In Output    eating cucumbers
    ${lc}=          Get Line Count   ${outlines}
    Should Be True      ${lc} == 2
    Verify Lines In Output      ${outlines}

    ${givens}=    Find String In Output    there are * cucumbers
    ${lc}=          Get Line Count   ${givens}
    Should Be True      ${lc} == 2
    Verify Lines In Output      ${givens}

*** Keywords ***
Find String In Output
    [Arguments]     ${find_string}
    ${output}=      Grep File        test.out    ${find_string}
    RETURN      ${output}

Verify Lines In Output
    [Arguments]     ${output}
    ${lines}=    Split To Lines    ${output}
    ${line_0}=   Strip String    ${lines}[0]
    ${line_1}=   Strip String    ${lines}[1]
    Should Not Be Equal      ${line_0}    ${line_1}
