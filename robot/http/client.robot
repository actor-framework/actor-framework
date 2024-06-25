*** Settings ***
Documentation       A test suite for the http client functionality

Library             Process
Library             String
Library             HttpCtrl.Server

Test Setup          Initialize HTTP Server
Test Teardown       Terminate HTTP Server


*** Variables ***
${HTTP_URL}         http://localhost:55516
${BINARY_PATH}      /path/to/the/client

${RES1}=            SEPARATOR=\n
...                 Server responded with HTTP 200: OK
...                 Header fields:
...                 - Server: HttpCtrl.Server/
...                 - Date:

${RES2}=            SEPARATOR=\n
...                 Server responded with HTTP 200: OK
...                 Header fields:
...                 - Server: HttpCtrl.Server/
...                 - Date:
...                 - Content-Length: 25
...                 Payload: { "status": "accepted" }
...                 ${EMPTY}

${RES3}=            SEPARATOR=\n
...                 Server responded with HTTP 200: OK
...                 Header fields:
...                 - Server: HttpCtrl.Server/
...                 - Date:
...                 - Content-Length: 8
...                 Payload:
...                 FF00FF00FF00FF00


*** Test Cases ***
Send GET request to server and receive UTF8 payload
    Send Request
    Wait For Request
    Reply By    200    { "status": "accepted" }\n
    Check Request Equals    GET    /    ${None}
    ${response}=    Await Response
    Should Be Equal As Strings    ${response}    ${RES2}

Send GET request to server and receive binary payload
    Send Request
    Wait For Request
    ${data}=    Convert To Bytes    FF 00 FF 00 FF 00 FF 00    input_type=hex
    Reply By    200    ${data}
    Check Request Equals    GET    /    ${None}
    ${response}=    Await Response
    Should Be Equal As Strings    ${response}    ${RES3}

Send HEAD request to server
    Send Request    -m    head
    Wait For Request
    Reply By    200
    Check Request Equals    HEAD    /    ${None}
    ${response}=    Await Response
    Should Be Equal As Strings    ${response}    ${RES1}

Send POST request to server with a payload
    Send Request    -m    post    -p    "Lorem ipsum"
    Wait For Request
    Reply By    200
    Check Request Equals    POST    /    "Lorem ipsum"
    ${response}=    Await Response
    Should Be Equal As Strings    ${response}    ${RES1}

Send PUT request to server with a payload
    Send Request    -m    put    -p    "Lorem ipsum"
    Wait For Request
    Reply By    200
    Check Request Equals    PUT    /    "Lorem ipsum"
    ${response}=    Await Response
    Should Be Equal As Strings    ${response}    ${RES1}


*** Keywords ***
Initialize HTTP Server
    Start Server    127.0.0.1    55516

Terminate HTTP Server
    Stop Server

Send Request
    [Arguments]    @{args}
    Start Process    ${BINARY_PATH}    @{args}    ${HTTP_URL}    alias=client

Await Response
    ${result}=    Wait For Process    client
    ${output}=    Replace String Using Regexp    ${result.stdout}    - Date: .*    - Date:
    RETURN    ${output}

Check Request Equals
    [Arguments]    ${expected_method}    ${expected_url}    ${expected_body}
    ${method}=    Get Request Method
    ${url}=    Get Request Url
    ${body}=    Get Request Body
    Should Be Equal    ${method}    ${expected_method}
    Should Be Equal    ${url}    ${expected_url}
    Should Be Equal As Strings    ${body}    ${expected_body}
