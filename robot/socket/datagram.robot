*** Settings ***
Documentation       A test suite for datagram sockets.

Library             Process


*** Variables ***
# Remote Actor.
${BINARY_PATH}          /path/to/the/test/binary
${UDP_SERVER_PORT}      55518
${UDP_BASELINE}=        "Lorem ipsum dolor sit amet."


*** Test Cases ***
Test sending simple message over udp
    [Tags]    socket    datagram
    Start Process    ${BINARY_PATH}    -l    ${UDP_SERVER_PORT}    alias=listener
    Run Process    ${BINARY_PATH}    -a    127.0.0.1    -p    ${UDP_SERVER_PORT}    stdin=${UDP_BASELINE}
    ${result}=    Terminate Process    listener
    Should Be Equal As Strings    ${UDP_BASELINE}    ${result.stdout}    formatter=repr
