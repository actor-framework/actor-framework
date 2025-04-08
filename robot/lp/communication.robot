*** Settings ***
Documentation       A test suite for the length prefix framing communication

Library             Process
Library             String
Library             OperatingSystem

Test Setup          Start Chat Server
Test Teardown       Terminate All Processes


*** Variables ***
${SERVER_HOST}      localhost
${SERVER_PORT}      55519
${SERVER_PATH}      /path/to/the/server
${CLIENT_PATH}      /path/to/the/client

${INPUT}            Hello\nToday is a wonderful day!\n
${BASELINE}         bob: Hello\nbob: Today is a wonderful day!
${ERROR}            connection error: logic_error("received empty buffer from stream layer")


*** Test Cases ***
Send Some message
    Start Chat Client    alice    u4
    Wait For Client    ${1}
    ${client_process}    Start Chat Client    bob    u4
    Wait For Client    ${2}
    Send Message    ${client_process}    ${INPUT}
    Wait Until Contains Baseline  alice.out

Open a client with different lp size
    ${client_process}    Start Chat Client    bob    u8
    Wait For Client    ${1}
    Send Message    ${client_process}    ${INPUT}
    Wait Until Contains Error  server.out

*** Keywords ***
Start Chat Server
    Start Process
    ...  ${SERVER_PATH}
    ...  -p    ${SERVER_PORT}
    ...  --caf.logger.file.verbosity  trace
    ...  --caf.logger.file.path  server.log
    ...  stdout=server.out
    ...  stderr=server.err
    Wait For Server Startup

Start Chat Client
    [Arguments]    ${name}  ${size}
    ${process}    Start Process
    ...  ${CLIENT_PATH}
    ...  -p    ${SERVER_PORT}
    ...  -n    ${name}
    ...  -s    ${size}
    ...  --caf.logger.file.verbosity  trace
    ...  --caf.logger.file.path  ${name}.log
    ...  stdin=PIPE
    ...  stdout=${name}.out
    ...  stderr=${name}.err
    RETURN    ${process}
    
Send Message
    [Arguments]    ${process}   ${message}
    Evaluate    $process.stdin.write($message.encode())
    Evaluate    $process.stdin.flush()

Has Baseline
    [Arguments]     ${file_path}
    ${output}       Get File      ${file_path}
    Should Contain  ${output}     ${BASELINE}

Has Error
    [Arguments]     ${file_path}
    ${output}       Get File      ${file_path}
    Should Contain  ${output}     ${ERROR}

Wait Until Contains Baseline
    [Arguments]     ${file_path}
    Wait Until Keyword Succeeds    5s    125ms    Has Baseline    ${file_path}

Wait Until Contains Error
    [Arguments]     ${file_path}
    Wait Until Keyword Succeeds    5s    125ms    Has Error    ${file_path}

Count Connected Clients
    [Arguments]     ${client_count}
    ${output}=      Grep File        server.out    accepted new connection
    ${lc}=          Get Line Count   ${output}
    Should Be True  ${lc} >= ${client_count}

Wait For Client
    [Arguments]    ${client_count}
    Wait Until Keyword Succeeds    5s    125ms    Count Connected Clients    ${client_count}

Has Server Started
    ${output}=      Grep File        server.out    server started
    Should Not Be Empty  ${output}

Wait For Server Startup
    Wait Until Keyword Succeeds    5s    125ms    Has Server Started
