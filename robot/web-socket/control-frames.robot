*** Settings ***
Documentation     A test suite for examples/web_socket/echo-server
Library           Collections
Library           Process
Library           RequestsLibrary
Library           WebSocketClient

*** Variables ***
${HTTP_URL}       http://localhost:55520
${HTTPS_URL}      https://localhost:55521
${WS_URL}         ws://localhost:55520
${WSS_URL}        wss://localhost:55521
${FRAME_COUNT}    ${10}
${BINARY_PATH}    /path/to/the/server
${SSL_PATH}       /path/to/the/pem/files

*** Test Cases ***

Test WebSocket Close Frame On Server Shutdown
    [Tags]     WebSocket
    [Timeout]  10
    ${system}=    Evaluate    platform.system()    platform
    Skip If  "${system}" == "Windows"  "Graceful shutdown via 'Terminate Process' doesn't work on Windows"
    ${server}=    Start HTTP Server
    ${fd}=                  WebSocketClient.Connect   ${WS_URL}
    Terminate Process    ${server}
    ${result}=              WebSocketClient.RecvData  ${fd}
    # Expect a close frame (opcode 8) without the status code and message.
    ${expected}=            Evaluate   tuple([8, b''])
    Should Be Equal         ${result}  ${expected}

Test WebSocket Close Frame On SSL Server Shutdown
    [Tags]     WebSocket
    [Timeout]  10
    ${system}=    Evaluate    platform.system()    platform
    Skip If  "${system}" == "Windows"  "Graceful shutdown via 'Terminate Process' doesn't work on Windows"
    ${server}=    Start HTTPS Server
    ${ssl_arg}=             Evaluate      {"cert_reqs": ssl.CERT_NONE}
    ${fd}=                  WebSocketClient.Connect  ${WSS_URL}   sslopt=${ssl_arg}
    Terminate Process    ${server}
    ${result}=              WebSocketClient.RecvData  ${fd}
    # Expect a close frame (opcode 8) without the status code and message.
    ${expected}=            Evaluate   tuple([8, b''])
    Should Be Equal         ${result}  ${expected}

*** Keywords ***
Start HTTP Server
    ${result}=    Start Process    ${BINARY_PATH}  -p  55520
    Wait Until Keyword Succeeds    5s    125ms    Check If HTTP Server Is Reachable
    RETURN    ${result}

Start HTTPS Server
    ${result}=    Start Process    ${BINARY_PATH}  -p  55521  -k  ${SSL_PATH}/key.pem  -c  ${SSL_PATH}/cert.pem
    Wait Until Keyword Succeeds    5s    125ms    Check If HTTPS Server Is Reachable
    RETURN    ${result}

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
