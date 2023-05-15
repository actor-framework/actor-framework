*** Settings ***
Documentation     A test suite for examples/web_socket/quote-server
Library           Collections
Library           Process
Library           RequestsLibrary
Library           WebSocketClient

Suite Setup       Start Servers
Suite Teardown    Stop Servers

*** Variables ***
${HTTP_URL}       http://localhost:55505
${HTTPS_URL}      https://localhost:55506
${WS_URL}         ws://localhost:55505/ws/quotes/seneca
${WSS_URL}        wss://localhost:55506/ws/quotes/seneca
${FRAME_COUNT}    ${10}
${BINARY_PATH}    /path/to/the/server
${SSL_PATH}       /path/to/the/pem/files
@{QUOTES}         Luck is what happens when preparation meets opportunity.
...               All cruelty springs from weakness.
...               We suffer more often in imagination than in reality.
...               Difficulties strengthen the mind, as labor does the body.
...               If a man knows not to which port he sails, no wind is favorable.
...               It is the power of the mind to be unconquerable.
...               No man was ever wise by chance.
...               He suffers more than necessary, who suffers before it is necessary.
...               I shall never be ashamed of citing a bad author if the line is good.
...               Only time can heal what reason cannot.

*** Test Cases ***
Test WebSocket Server Quotes
    [Tags]    WebSocket
    ${fd}=                  WebSocketClient.Connect  ${WS_URL}
    ${received_strings}=    Receive Frames           ${fd}    ${FRAME_COUNT}
    WebSocketClient.Close   ${fd}
    Validate Frames         ${received_strings}

Test WebSocket Over SSL Server Quotes
    [Tags]    WebSocket
    ${ssl_arg}=             Evaluate      {"cert_reqs": ssl.CERT_NONE}
    ${fd}=                  WebSocketClient.Connect  ${WSS_URL}   sslopt=${ssl_arg}
    ${received_strings}=    Receive Frames           ${fd}    ${FRAME_COUNT}
    WebSocketClient.Close   ${fd}
    Validate Frames         ${received_strings}

*** Keywords ***
Start Servers
    Start Process    ${BINARY_PATH}  -p  55505
    Start Process    ${BINARY_PATH}  -p  55506  -k  ${SSL_PATH}/key.pem  -c  ${SSL_PATH}/cert.pem
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

Receive Frames
    [Arguments]    ${fd}    ${frame_count}
    @{received_strings}=    Create List
    FOR    ${i}    IN RANGE    ${frame_count}
        ${frame}=         WebSocketClient.Recv    ${fd}
        Append To List    ${received_strings}     ${frame}
    END
    [Return]    ${received_strings}

Validate Frames
    [Arguments]    ${received_strings}
    Log            Received: ${received_strings}.
    ${count}=      Get Length    ${received_strings}
    Should Be Equal As Integers  ${count}              ${FRAME_COUNT}
    Lists Should Be Equal        ${received_strings}   ${QUOTES}
