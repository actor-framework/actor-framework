*** Settings ***
Documentation     A test suite for examples/web_socket/echo-server
Library           Collections
Library           Process
Library           RequestsLibrary
Library           WebSocketClient

Suite Setup       Start Servers
Suite Teardown    Stop Servers

*** Variables ***
${HTTP_URL}       http://localhost:55501
${HTTPS_URL}      https://localhost:55502
${WS_URL}         ws://localhost:55501
${WSS_URL}        wss://localhost:55502
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
    ${res1}=    Start Process    ${BINARY_PATH}  -p  55501
    Set Suite Variable    ${ws_server_process}    ${res1}
    ${res2}=    Start Process    ${BINARY_PATH}  -p  55502  -k  ${SSL_PATH}/key.pem  -c  ${SSL_PATH}/cert.pem
    Set Suite Variable    ${wss_server_process}    ${res2}
    Wait Until Keyword Succeeds    5s    125ms    Check If HTTP Server Is Reachable
    Wait Until Keyword Succeeds    5s    125ms    Check If HTTPS Server Is Reachable

Stop Servers
    Terminate Process    ${ws_server_process}
    Terminate Process    ${wss_server_process}

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
