*** Settings ***
Documentation     A test suite for --dump-config.
Library           OperatingSystem
Library           Process

*** Variables ***
${BINARY_PATH}    /path/to/the/test/binary

*** Test Cases ***
The output of --dump-config should generate valid input for --config-file
    [Tags]    Config
    Run Process  ${BINARY_PATH}  --dump-config  stdout=out1.cfg
    Run Process  ${BINARY_PATH}  --config-file\=out1.cfg  --dump-config  stdout=out2.cfg
    ${out1}=     Get File    out1.cfg
    ${out2}=     Get File    out2.cfg
    Should Be Equal As Strings    ${out1}    ${out2}
