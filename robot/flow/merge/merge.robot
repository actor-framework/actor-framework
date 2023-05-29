*** Settings ***
Documentation       A test suite for stress-testing the merge operator.

Library             OperatingSystem
Library             Process


*** Variables ***
${BINARY_PATH}          /path/to/the/test/binary

# Config1: one fast publishers, one fast subscriber.

${CFG1_P}=             -p [{init: 1, num: 10000}]
${CFG1_S}=             -s [{}]

${RES1}=                SEPARATOR=\n
...                     subscriber-0: [1-10000]
...

# Config2: one fast publishers, 10 fast subscribers.

${CFG2_P}=             -p [{init: 1, num: 10000}]
${CFG2_S}=             -s [{},{},{},{},{},{},{},{},{},{}]

${RES2}=                SEPARATOR=\n
...                     subscriber-0: [1-10000]
...                     subscriber-1: [1-10000]
...                     subscriber-2: [1-10000]
...                     subscriber-3: [1-10000]
...                     subscriber-4: [1-10000]
...                     subscriber-5: [1-10000]
...                     subscriber-6: [1-10000]
...                     subscriber-7: [1-10000]
...                     subscriber-8: [1-10000]
...                     subscriber-9: [1-10000]
...

# Config3: 3 fast publishers, one fast subscriber.

${CFG3_P}=             -p [{init: 1, num: 10000},{init: 10001, num: 10000},{init: 20001, num: 10000}]
${CFG3_S}=             -s [{}]

${RES3}=                SEPARATOR=\n
...                     subscriber-0: [1-30000]
...

# Config4: one fast publisher, 5 fast subscribers with different cutoffs.

${CFG4_P}=             -p [{init: 1, num: 10000}]
${CFG4_S}=             -s [{},{limit=8000},{limit=6000},{limit=4000},{limit=2000}]

${RES4}=                SEPARATOR=\n
...                     subscriber-0: [1-10000]
...                     subscriber-1: [1-8000]
...                     subscriber-2: [1-6000]
...                     subscriber-3: [1-4000]
...                     subscriber-4: [1-2000]
...

# Config5: one fast publisher, 5 subscribers with different cutoffs and delays.

${CFG5_P}=             -p [{init: 1, num: 10000}]
${CFG5_S}=             -s [{},{limit=8000,delay=10us},{limit=6000,delay=30us},{limit=4000,delay=40us},{limit=2000,delay=7us}]

${RES5}=                SEPARATOR=\n
...                     subscriber-0: [1-10000]
...                     subscriber-1: [1-8000]
...                     subscriber-2: [1-6000]
...                     subscriber-3: [1-4000]
...                     subscriber-4: [1-2000]
...

# Config6: two publishers with different delays, one subscriber

${CFG6_P}=             -p [{init: 1, num: 10000, delay: 7us}, {init: 10001, num: 10000, delay: 13us}]
${CFG6_S}=             -s [{}]

${RES6}=                SEPARATOR=\n
...                     subscriber-0: [1-20000]
...

*** Test Cases ***
One Fast Publisher, One Fast Subscriber
    [Tags]    flow
    Remove Files    app.out  app.err
    Run Process    ${BINARY_PATH}  ${CFG1_P}  ${CFG1_S}  stdout=app.out  stderr=app.err
    ${app_err}=    Get File    app.err
    Should Be Empty  ${app_err}
    ${app_out}=    Get File    app.out
    Should Be Equal As Strings  ${RES1}  ${app_out}  formatter=repr

One Fast Publisher, 10 Fast Subscribers
    [Tags]    flow
    Remove Files    app.out  app.err
    Run Process    ${BINARY_PATH}  ${CFG2_P}  ${CFG2_S}  stdout=app.out  stderr=app.err
    ${app_err}=    Get File    app.err
    Should Be Empty  ${app_err}
    ${app_out}=    Get File    app.out
    Should Be Equal As Strings  ${RES2}  ${app_out}  formatter=repr

3 Fast Publishers, One Fast Subscriber
    [Tags]    flow
    Remove Files    app.out  app.err
    Run Process    ${BINARY_PATH}  ${CFG3_P}  ${CFG3_S}  stdout=app.out  stderr=app.err
    ${app_err}=    Get File    app.err
    Should Be Empty  ${app_err}
    ${app_out}=    Get File    app.out
    Should Be Equal As Strings  ${RES3}  ${app_out}  formatter=repr

One Fast Publisher, 5 Fast Subscribers with Different Cutoffs
    [Tags]    flow
    Remove Files    app.out  app.err
    Run Process    ${BINARY_PATH}  ${CFG4_P}  ${CFG4_S}  stdout=app.out  stderr=app.err
    ${app_err}=    Get File    app.err
    Should Be Empty  ${app_err}
    ${app_out}=    Get File    app.out
    Should Be Equal As Strings  ${RES4}  ${app_out}  formatter=repr

One Fast Publishers, 5 Subscribers with Different Cutoffs and Delays
    [Tags]    flow
    Remove Files    app.out  app.err
    Run Process    ${BINARY_PATH}  ${CFG5_P}  ${CFG5_S}  stdout=app.out  stderr=app.err
    ${app_err}=    Get File    app.err
    Should Be Empty  ${app_err}
    ${app_out}=    Get File    app.out
    Should Be Equal As Strings  ${RES5}  ${app_out}  formatter=repr

Two Publishers with Different Delays, One Subscriber
    [Tags]    flow
    Remove Files    app.out  app.err
    Run Process    ${BINARY_PATH}  ${CFG6_P}  ${CFG6_S}  stdout=app.out  stderr=app.err
    ${app_err}=    Get File    app.err
    Should Be Empty  ${app_err}
    ${app_out}=    Get File    app.out
    Should Be Equal As Strings  ${RES6}  ${app_out}  formatter=repr
