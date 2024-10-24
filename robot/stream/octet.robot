*** Settings ***
Documentation       A test suite for examples/octet_stream/key-value-store

Library             Process
Library             String
Library             OperatingSystem

Test Setup          Start Key Value Store Server
Test Teardown       Stop Server And Clients


*** Variables ***
${SERVER_HOST}      localhost
${SERVER_PORT}      55520
${SERVER_PATH}      /path/to/the/server
${CLIENT_PATH}      /path/to/the/client

${FIRST_CLIENT_INPUT}         { "type": "put", "key": "A", "value": "26" }\n{ "type": "get", "key": "A" }\n{ "type": "del", "key": "A" }\n{ "type": "get", "key": "A" }\n
${FIRST_CLIENT_BASELINE}      reply: \nreply: 26\nreply: 26\nreply: {"error":"no_such_key"}
${SECOND_CLIENT_INPUT}        { "type": "get", "key": "A" }\n{ "type": "put", "key": "B", "value": "27" }\n{ "type": "get", "key": "B" }\n{ "type": "del", "key": "B" }\n{ "type": "get", "key": "B" }\n
${SECOND_CLIENT_BASELINE}     reply: {"error":"no_such_key"}\nreply: \nreply: 27\nreply: 27\nreply: {"error":"no_such_key"}


*** Test Cases ***
The server can handle multiple clients
    ${c1}=  Start Text Client
    Wait For Client    ${1}
    Send Message  ${c1}  ${FIRST_CLIENT_INPUT}
    Wait Until Contains Baseline  ${FIRST_CLIENT_BASELINE}  text-store.out
    Terminate Process  ${c1}
    ${c2}=  Start Text Client
    Wait For Client    ${2}
    Send Message  ${c2}  ${SECOND_CLIENT_INPUT}
    Wait Until Contains Baseline  ${SECOND_CLIENT_BASELINE}  text-store.out
    Terminate Process  ${c2}


*** Keywords ***
Start Key Value Store Server
    Start Process
    ...  ${SERVER_PATH}
    ...  -p    ${SERVER_PORT}
    ...  --caf.logger.file.path  server.log
    ...  stdout=server.out
    ...  stderr=server.err
    Wait For Server Startup

Start Text Client
    ${hdl}=  Start Process
    ...  ${CLIENT_PATH}
    ...  -p    ${SERVER_PORT}
    ...  --caf.logger.file.path  text-store.log
    ...  stdin=PIPE
    ...  stdout=text-store.out
    ...  stderr=text-store.err
    RETURN    ${hdl}

Send Message
    [Arguments]    ${process}   ${message}
    Evaluate    $process.stdin.write($message.encode())
    Evaluate    $process.stdin.flush()

Has Baseline
    [Arguments]     ${baseline}   ${file_path}
    ${output}       Get File      ${file_path}
    Should Contain  ${output}     ${baseline}

Wait Until Contains Baseline
    [Arguments]     ${baseline}   ${file_path}
    Wait Until Keyword Succeeds    5s    125ms    Has Baseline    ${baseline}   ${file_path}

Count Connected Clients
    [Arguments]     ${client_count}
    ${output}=      Grep File        server.out    accepted new connection
    ${lc}=          Get Line Count   ${output}
    Should Be True  ${lc} >= ${client_count}

Wait For Client
    [Arguments]    ${client_count}
    Wait Until Keyword Succeeds    5s    125ms    Count Connected Clients    ${client_count}

Has Server Started
    ${output}=      Grep File        server.out    server is running
    Should Not Be Empty  ${output}

Wait For Server Startup
    Wait Until Keyword Succeeds    5s    125ms    Has Server Started

Stop Server And Clients
    Run Keyword And Ignore Error    Terminate All Processes
