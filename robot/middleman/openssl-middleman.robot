*** Settings ***
Documentation     A test suite for remote streams.

Library           Collections
Library           OperatingSystem
Library           Process
Library           RequestsLibrary
Library           String


*** Variables ***
${SSL_PATH}         /path/to/the/tls/files

# Remote Actor.
${RA_SERVER_PORT}    55516
${RA_BASELINE}=      SEPARATOR=
...                  cell value 1: 42\n
...                  cell value 2: 49\n
...                  cell down\n

# Unpublish.
${UN_SERVER_PORT}    55517
${UN_BASELINE}=      SEPARATOR=
...                  unpublish success\n

# Authentication.
${AN_SERVER_PORT}    55518
${AN_BASELINE}=      failed to connect: disconnect_during_handshake\n


*** Test Cases ***
Test Publish and Remote Actor without Authentication
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  remote_actor  -p  ${RA_SERVER_PORT}
    Run Process    ${BINARY_PATH}  -m  remote_actor  -p  ${RA_SERVER_PORT}  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${RA_BASELINE}  ${found}  formatter=repr

Test Publish and Remote Actor with Authentication
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  remote_actor  -p  ${RA_SERVER_PORT}  --caf.openssl.passphrase  12345  --caf.openssl.certificate  ${SSL_PATH}/cert.pem  --caf.openssl.key  ${SSL_PATH}/key.pem
    Run Process    ${BINARY_PATH}  -m  remote_actor  -p  ${RA_SERVER_PORT}  -caf.openssl.cafile   ${SSL_PATH}/ca.pem  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${RA_BASELINE}  ${found}  formatter=repr

Test Publish and Unpublish
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  unpublish  -p  ${UN_SERVER_PORT}
    Run Process    ${BINARY_PATH}  -m  unpublish  -p  ${UN_SERVER_PORT}  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${UN_BASELINE}  ${found}  formatter=repr

Authentication Failure
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  remote_actor  -p  ${AN_SERVER_PORT}  --caf.openssl.passphrase  12345  --caf.openssl.certificate  ${SSL_PATH}/cert.pem  --caf.openssl.key  ${SSL_PATH}/key.pem
    Run Process    ${BINARY_PATH}  -m  remote_actor  -p  ${AN_SERVER_PORT}  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${AN_BASELINE}  ${found}  formatter=repr
