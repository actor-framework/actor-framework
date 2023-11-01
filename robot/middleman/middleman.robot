*** Settings ***
Documentation     A test suite for remote streams.

Library           Collections
Library           OperatingSystem
Library           Process
Library           RequestsLibrary
Library           String


*** Variables ***
# Remote Actor.
${RA_SERVER_PORT}    55508
${RA_BASELINE}=      SEPARATOR=
...                  cell value 1: 42\n
...                  cell value 2: 49\n
...                  cell down\n

# Remote Spawn.
${RS_SERVER_PORT}    55509
${RS_BASELINE}=      SEPARATOR=
...                  cell value 1: 7\n
...                  cell value 2: 14\n
...                  cell down\n

# Remote Lookup.
${RL_SERVER_PORT}    55510
${RL_BASELINE}=      SEPARATOR=
...                  cell value 1: 23\n
...                  cell value 2: 30\n
...                  cell down\n

# Unpublish.
${UN_SERVER_PORT}    55511
${UN_BASELINE}=      SEPARATOR=
...                  unpublish success\n

# Monitor Node.
${MN_SERVER_PORT}    55512
${MN_BASELINE}=      SEPARATOR=
...                  server down\n

# Deserialization Error.
${DE_SERVER_PORT}    55513
${DE_BASELINE}=      SEPARATOR=
...                  error: malformed_message\n

# Prometheus.
${PR_SERVER_PORT}    55514
${PR_HTTP_URL}       http://localhost:55514/metrics
${PR_HTTP_BAD_URL}   http://localhost:55514/foobar

# Ping-Pong.
${PP_SERVER_PORT}    55515
${PP_BASELINE}=      SEPARATOR=
...                  got pong\n
...                  pong down\n


*** Test Cases ***
Test Publish and Remote Actor
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  remote_actor  -p  ${RA_SERVER_PORT}
    Run Process    ${BINARY_PATH}  -m  remote_actor  -p  ${RA_SERVER_PORT}  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${RA_BASELINE}  ${found}  formatter=repr

Test Connect and Remote Spawn
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  remote_spawn  -p  ${RS_SERVER_PORT}
    Run Process    ${BINARY_PATH}  -m  remote_spawn  -p  ${RS_SERVER_PORT}  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${RS_BASELINE}  ${found}  formatter=repr

Test Connect and Remote Lookup
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  remote_lookup  -p  ${RL_SERVER_PORT}
    Run Process    ${BINARY_PATH}  -m  remote_lookup  -p  ${RL_SERVER_PORT}  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${RL_BASELINE}  ${found}  formatter=repr

Test Publish and Unpublish
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  unpublish  -p  ${UN_SERVER_PORT}
    Run Process    ${BINARY_PATH}  -m  unpublish  -p  ${UN_SERVER_PORT}  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${UN_BASELINE}  ${found}  formatter=repr

Monitor Node
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  monitor_node  -p  ${MN_SERVER_PORT}
    Run Process    ${BINARY_PATH}  -m  monitor_node  -p  ${MN_SERVER_PORT}  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${MN_BASELINE}  ${found}  formatter=repr

Deserialization Error
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  deserialization_error  -p  ${DE_SERVER_PORT}
    Run Process    ${BINARY_PATH}  -m  deserialization_error  -p  ${DE_SERVER_PORT}  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${DE_BASELINE}  ${found}  formatter=repr

Prometheus
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  prometheus  --caf.middleman.prometheus-http.port  ${PR_SERVER_PORT}
    Wait Until Keyword Succeeds    5s    25ms    Check If Reachable  ${PR_HTTP_URL}
    ${resp}=     GET    ${PR_HTTP_URL}
    ${content}=  Decode Bytes To String  ${resp.content}  utf-8
    Should Contain  ${content}    caf_system_running_actors
    Run Keyword And Expect Error  *  GET  ${PR_HTTP_BAD_URL}

Indirect Connection Ping Pong
    [Tags]    Middleman
    [Teardown]    Terminate All Processes
    Start Process    ${BINARY_PATH}  -s  -m  rendesvous  -p  ${PP_SERVER_PORT}
    Start Process    ${BINARY_PATH}  -m  pong  -p  ${PP_SERVER_PORT}
    Run Process    ${BINARY_PATH}  -m  ping  -p  ${PP_SERVER_PORT}  stdout=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings  ${PP_BASELINE}  ${found}  formatter=repr


*** Keywords ***
Check If Reachable
    [Arguments]    ${url}
    Log         Try reaching ${url}.
    GET    ${url}    expected_status=200
