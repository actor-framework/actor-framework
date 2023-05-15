*** Settings ***
Documentation     A test suite for examples/http/rest.hpp.
Library           Process
Library           RequestsLibrary

Suite Setup       Start Servers
Suite Teardown    Stop Servers

*** Variables ***
${HTTP_URL}       http://localhost:55501
${HTTPS_URL}      https://localhost:55502
${BINARY_PATH}    /path/to/the/server
${SSL_PATH}       /path/to/the/pem/files

*** Test Cases ***
HTTP Test Add Key Value Pair
    [Tags]    POST
    Add Key Value Pair    ${HTTP_URL}    foo    bar
    Get Key Value Pair    ${HTTP_URL}    foo    bar

HTTP Test Update Key Value Pair
    [Tags]    POST
    Add Key Value Pair       ${HTTP_URL}    foo    bar
    Update Key Value Pair    ${HTTP_URL}    foo    baz
    Get Key Value Pair       ${HTTP_URL}    foo    baz

HTTP Test Delete Key Value Pair
    [Tags]    DELETE
    Add Key Value Pair       ${HTTP_URL}    foo    bar
    Delete Key Value Pair    ${HTTP_URL}    foo
    Key Should Not Exist     ${HTTP_URL}    foo

HTTPS Test Add Key Value Pair
    [Tags]    POST
    Add Key Value Pair    ${HTTPS_URL}    foo    bar
    Get Key Value Pair    ${HTTPS_URL}    foo    bar

HTTPS Test Update Key Value Pair
    [Tags]    POST
    Add Key Value Pair       ${HTTPS_URL}    foo    bar
    Update Key Value Pair    ${HTTPS_URL}    foo    baz
    Get Key Value Pair       ${HTTPS_URL}    foo    baz

HTTPS Test Delete Key Value Pair
    [Tags]    DELETE
    Add Key Value Pair       ${HTTPS_URL}    foo    bar
    Delete Key Value Pair    ${HTTPS_URL}    foo
    Key Should Not Exist     ${HTTPS_URL}    foo

*** Keywords ***
Start Servers
    Start Process    ${BINARY_PATH}  -p  55501
    Start Process    ${BINARY_PATH}  -p  55502  -k  ${SSL_PATH}/key.pem  -c  ${SSL_PATH}/cert.pem
    Wait Until Keyword Succeeds    5s    125ms    Check If HTTP Server Is Reachable
    Wait Until Keyword Succeeds    5s    125ms    Check If HTTPS Server Is Reachable

Stop Servers
    Run Keyword And Ignore Error    Terminate All Processes

Check If HTTP Server Is Reachable
    Log         Try reaching ${HTTP_URL}/status.
    ${resp}=    GET    ${HTTP_URL}/status    expected_status=204

Check If HTTPS Server Is Reachable
    Log         Try reaching ${HTTPS_URL}/status.
    ${resp}=    GET    ${HTTPS_URL}/status    expected_status=204    verify=${False}

Add Key Value Pair
    [Arguments]    ${base_url}    ${key}    ${value}
    ${resp}=    POST    ${base_url}/api/${key}    data=${value}    expected_status=204    verify=${False}

Get Key Value Pair
    [Arguments]    ${base_url}    ${key}    ${expected_value}
    ${resp}=    GET     ${base_url}/api/${key}   expected_status=200    verify=${False}
    Should Be Equal As Strings    ${resp.content}    ${expected_value}

Update Key Value Pair
    [Arguments]    ${base_url}    ${key}    ${new_value}
    ${resp}=    POST     ${base_url}/api/${key}    data=${new_value}   expected_status=204    verify=${False}

Delete Key Value Pair
    [Arguments]    ${base_url}    ${key}
    ${resp}=    DELETE    ${base_url}/api/${key}   expected_status=204    verify=${False}

Key Should Not Exist
    [Arguments]    ${base_url}    ${key}
    ${resp}=    GET     ${base_url}/api/${key}   expected_status=404    verify=${False}
