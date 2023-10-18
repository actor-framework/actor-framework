*** Settings ***
Documentation       A test suite for --dump-config.

Library             OperatingSystem
Library             Process


*** Variables ***
${BINARY_PATH}          /path/to/the/test/binary

${TRACE_BASELINE}=      SEPARATOR=
...                     TRACE app ENTRY value = 42\n
...                     DEBUG app this is a debug message\n
...                     INFO app this is an info message\n
...                     WARN app this is a warning message\n
...                     ERROR app this is an error message\n
...                     TRACE app EXIT\n

${DEBUG_BASELINE}=      SEPARATOR=
...                     DEBUG app this is a debug message\n
...                     INFO app this is an info message\n
...                     WARN app this is a warning message\n
...                     ERROR app this is an error message\n

${INFO_BASELINE}=       SEPARATOR=
...                     INFO app this is an info message\n
...                     WARN app this is a warning message\n
...                     ERROR app this is an error message\n

${WARN_BASELINE}=       SEPARATOR=
...                     WARN app this is a warning message\n
...                     ERROR app this is an error message\n

${ERROR_BASELINE}=      SEPARATOR=
...                     ERROR app this is an error message\n


*** Keywords ***
Run Console Logger
    [Arguments]    ${verbosity}   ${api}
    Remove File    app.out
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.console.verbosity\=${verbosity}
    ...    --api\=${api}
    ...    stderr=app.out
    ${output}=    Get File    app.out
    [Return]    ${output}

Run File Logger
    [Arguments]    ${verbosity}   ${api}
    Remove File    app.log
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.file.verbosity\=${verbosity}
    ...    --api\=${api}

*** Test Cases ***
The console logger prints everything when setting the log level to TRACE
    [Tags]    logging
    ${found}=    Run Console Logger  trace  legacy
    Should Be Equal As Strings  ${TRACE_BASELINE}  ${found}  formatter=repr
    ${found}=    Run Console Logger  trace  default
    Should Be Equal As Strings  ${TRACE_BASELINE}  ${found}  formatter=repr

The file logger prints everything when setting the log level to TRACE
    [Tags]    logging
    Run File Logger  trace  legacy
    ${found}=    Get File    app.log
    Should Be Equal As Strings  ${TRACE_BASELINE}  ${found}  formatter=repr
    Run File Logger  trace  default
    ${found}=    Get File    app.log
    Should Be Equal As Strings  ${TRACE_BASELINE}  ${found}  formatter=repr

The console logger prints a subset when setting the log level to DEBUG
    [Tags]    logging
    ${found}=    Run Console Logger  debug  legacy
    Should Be Equal As Strings  ${DEBUG_BASELINE}  ${found}  formatter=repr
    ${found}=    Run Console Logger  debug  default
    Should Be Equal As Strings  ${DEBUG_BASELINE}  ${found}  formatter=repr

The file logger prints a subset when setting the log level to DEBUG
    [Tags]    logging
    Run File Logger  debug  legacy
    ${found}=    Get File    app.log
    Should Be Equal As Strings  ${DEBUG_BASELINE}  ${found}  formatter=repr
    Run File Logger  debug  default
    ${found}=    Get File    app.log
    Should Be Equal As Strings  ${DEBUG_BASELINE}  ${found}  formatter=repr

The console logger prints a subset when setting the log level to INFO
    [Tags]    logging
    ${found}=    Run Console Logger  info  legacy
    Should Be Equal As Strings  ${INFO_BASELINE}  ${found}  formatter=repr
    ${found}=    Run Console Logger  info  default
    Should Be Equal As Strings  ${INFO_BASELINE}  ${found}  formatter=repr

The file logger prints a subset when setting the log level to INFO
    [Tags]    logging
    Run File Logger  info  legacy
    ${found}=    Get File    app.log
    Should Be Equal As Strings  ${INFO_BASELINE}  ${found}  formatter=repr
    Run File Logger  info  default
    ${found}=    Get File    app.log
    Should Be Equal As Strings  ${INFO_BASELINE}  ${found}  formatter=repr

The console logger prints a subset when setting the log level to WARN
    [Tags]    logging
    ${found}=    Run Console Logger  warning  legacy
    Should Be Equal As Strings  ${WARN_BASELINE}  ${found}  formatter=repr
    ${found}=    Run Console Logger  warning  default
    Should Be Equal As Strings  ${WARN_BASELINE}  ${found}  formatter=repr

The file logger prints a subset when setting the log level to WARN
    [Tags]    logging
    Run File Logger  warning  legacy
    ${found}=    Get File    app.log
    Should Be Equal As Strings  ${WARN_BASELINE}  ${found}  formatter=repr
    Run File Logger  warning  default
    ${found}=    Get File    app.log
    Should Be Equal As Strings  ${WARN_BASELINE}  ${found}  formatter=repr

The console logger prints a subset when setting the log level to ERROR
    [Tags]    logging
    ${found}=    Run Console Logger  error  legacy
    Should Be Equal As Strings  ${ERROR_BASELINE}  ${found}  formatter=repr
    ${found}=    Run Console Logger  error  default
    Should Be Equal As Strings  ${ERROR_BASELINE}  ${found}  formatter=repr

The applications prints a subset when setting the log level to ERROR
    [Tags]    logging
    Run File Logger  error  legacy
    ${found}=    Get File    app.log
    Should Be Equal As Strings  ${ERROR_BASELINE}  ${found}  formatter=repr
    Run File Logger  error  default
    ${found}=    Get File    app.log
    Should Be Equal As Strings  ${ERROR_BASELINE}  ${found}  formatter=repr

The console logger prints nothing when setting the log level to QUIET
    [Tags]    logging
    ${found}=    Run Console Logger  quiet  legacy
    Should Be Empty    ${found}
    ${found}=    Run Console Logger  quiet  default
    Should Be Empty    ${found}

The file logger prints nothing when setting the log level to QUIET
    [Tags]    logging
    Run File Logger  quiet  legacy
    File Should Not Exist    app.log
    Run File Logger  quiet  default
    File Should Not Exist    app.log
