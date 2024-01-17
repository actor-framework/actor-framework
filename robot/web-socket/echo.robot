*** Settings ***
Documentation     A test suite for examples/web_socket/echo-server
Library           Collections
Library           Process
Library           RequestsLibrary
Library           WebSocketClient

Suite Setup       Start Servers
Suite Teardown    Stop Servers

*** Variables ***
${HTTP_URL}       http://localhost:55503
${HTTPS_URL}      https://localhost:55504
${WS_URL}         ws://localhost:55503
${WSS_URL}        wss://localhost:55504
${FRAME_COUNT}    ${10}
${BINARY_PATH}    /path/to/the/server
${SSL_PATH}       /path/to/the/pem/files

*** Test Cases ***
Test WebSocket Server
    [Tags]    WebSocket
    ${fd}=                  WebSocketClient.Connect  ${WS_URL}
    WebSocketClient.Send    ${fd}    Hello
    ${result}=              WebSocketClient.Recv    ${fd}
    WebSocketClient.Close   ${fd}
    Should Be Equal         ${result}    Hello

Test WebSocket Over SSL Server
    [Tags]    WebSocket
    ${ssl_arg}=             Evaluate      {"cert_reqs": ssl.CERT_NONE}
    ${fd}=                  WebSocketClient.Connect  ${WSS_URL}   sslopt=${ssl_arg}
    WebSocketClient.Send    ${fd}    Hello
    ${result}=              WebSocketClient.Recv    ${fd}
    WebSocketClient.Close   ${fd}
    Should Be Equal         ${result}    Hello

*** Keywords ***
Start Servers
    Start Process    ${BINARY_PATH}  -p  55503
    Start Process    ${BINARY_PATH}  -p  55504  -k  ${SSL_PATH}/key.pem  -c  ${SSL_PATH}/cert.pem
    Wait Until Keyword Succeeds    5s    125ms    Check If HTTP Server Is Reachable
    Wait Until Keyword Succeeds    5s    125ms    Check If HTTPS Server Is Reachable

Stop Servers
    Run Keyword And Ignore Error    Terminate All Processes

Check If HTTP Server Is Reachable
    # The server sends a "400 Bad Request" if we try HTTP on the WebSocket port.
    # This is because the server is configured to only accept WebSocket
    # connections. However, we can still use this behavior to check if the
    # server is up and running.
    Log         Try reaching ${HTTP_URL}.
    ${resp}=    GET    ${HTTP_URL}    expected_status=400

Check If HTTPS Server Is Reachable
    Log         Try reaching ${HTTPS_URL}.
    ${resp}=    GET    ${HTTPS_URL}    expected_status=400    verify=${False}
