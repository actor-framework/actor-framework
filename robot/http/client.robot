*** Settings ***

Documentation     A test suite for the http client functionality
Library           Process
Library           HttpCtrl.Server

Test Setup        Initialize HTTP Server
Test Teardown     Terminate HTTP Server

*** Variables ***

${BINARY_PATH}    /path/to/the/client

*** Test Cases ***

Send GET request to server
    Send Request
    Wait For Request
    Reply By            200
    Check Request Equals     GET     /       ${None}

Send POST request to server
    Send Request    -m   post
    Wait For Request
    Reply By            200
    Check Request Equals     POST    /       ${None}

*** Keywords ***

Initialize HTTP Server
    Start Server        127.0.0.1   8000

Terminate HTTP Server
    Stop Server

Send Request
    [Arguments]    @{args} 
    Start Process    ${BINARY_PATH}     @{args}

Check Request Equals
    [Arguments]    ${expected_method}    ${expected_url}    ${expected_body}
    ${method}=          Get Request Method
    ${url}=             Get Request Url
    ${body}=            Get Request Body
    Should Be Equal     ${method}   ${expected_method}
    Should Be Equal     ${url}      ${expected_url}
    Should Be Equal     ${body}     ${expected_body}

