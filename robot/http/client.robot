*** Settings ***
Documentation       A test suite for the http client functionality

Library             Process
Library             HttpCtrl.Server

Test Setup          Initialize HTTP Server
Test Teardown       Terminate HTTP Server


*** Variables ***
${HTTP_URL}         http://localhost:55516
${BINARY_PATH}      /path/to/the/client

${RES1}=            SEPARATOR=\n
...                 HTTP/1.0 200 OK
...                 Server:HttpCtrl.Server/
...                 ${EMPTY}

${RES2}=            SEPARATOR=\n
...                 HTTP/1.0 200 OK
...                 Server:HttpCtrl.Server/
...                 Content-Length:25
...                 { "status": "accepted" }
...                 ${EMPTY}


*** Test Cases ***
Send GET request to server
    Send Request
    Wait For Request
    Reply By    200    { "status": "accepted" }\n
    Check Request Equals    GET    /    ${None}
    ${response}=    Await Response
    Should Be Equal As Strings    ${response}    ${RES2}

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
    Start Process    ${BINARY_PATH}    -r    ${HTTP_URL}    @{args}    alias=client

Await Response
    ${result}=    Wait For Process    client
    RETURN    ${result.stdout}

Check Request Equals
    [Arguments]    ${expected_method}    ${expected_url}    ${expected_body}
    ${method}=    Get Request Method
    ${url}=    Get Request Url
    ${body}=    Get Request Body
    Should Be Equal    ${method}    ${expected_method}
    Should Be Equal    ${url}    ${expected_url}
    Should Be Equal As Strings    ${body}    ${expected_body}
