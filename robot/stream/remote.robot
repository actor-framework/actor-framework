*** Settings ***
Documentation     A test suite for remote streams.

Library           Collections
Library           Process
Library           OperatingSystem


*** Variables ***
${SERVER_PORT}    55507
${BASELINE}=      SEPARATOR=
...               point(1, 1)\n
...               point(2, 4)\n
...               point(3, 9)\n
...               point(4, 16)\n
...               point(5, 25)\n
...               point(6, 36)\n
...               point(7, 49)\n
...               point(8, 64)\n
...               point(9, 81)\n


*** Test Cases ***
Test Remote Streams
    [Tags]    Stream
    Start Process    ${BINARY_PATH}  -s  -p  ${SERVER_PORT}
    Run Process    ${BINARY_PATH}  -p  ${SERVER_PORT}  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${BASELINE}  ${found}  formatter=repr
    Run Keyword And Ignore Error    Terminate All Processes
