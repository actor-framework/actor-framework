*** Settings ***
Documentation       A test suite for --dump-config.

Library             OperatingSystem
Library             Process


*** Variables ***
${BINARY_PATH}          /path/to/the/test/binary

${TRACE_BASELINE}=      SEPARATOR=
...                     TRACE app ENTRY value = 42${\n}
...                     DEBUG app this is a debug message${\n}
...                     INFO app this is an info message${\n}
...                     WARN app this is a warning message${\n}
...                     ERROR app this is an error message${\n}
...                     TRACE app EXIT${\n}

${DEBUG_BASELINE}=      SEPARATOR=
...                     DEBUG app this is a debug message${\n}
...                     INFO app this is an info message${\n}
...                     WARN app this is a warning message${\n}
...                     ERROR app this is an error message${\n}

${INFO_BASELINE}=       SEPARATOR=
...                     INFO app this is an info message${\n}
...                     WARN app this is a warning message${\n}
...                     ERROR app this is an error message${\n}

${WARN_BASELINE}=       SEPARATOR=
...                     WARN app this is a warning message${\n}
...                     ERROR app this is an error message${\n}

${ERROR_BASELINE}=      SEPARATOR=
...                     ERROR app this is an error message${\n}


*** Test Cases ***
The applications logs everything when setting the log level to TRACE
    [Tags]    logging
    Remove Files    app.out    app.log
    LOG    "Print CAF log to stdout"
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.console.verbosity\=trace
    ...    stderr=app.out
    ${found}=    Get File    app.out
    LOG    "Print CAF log to a file"
    Should Be Equal As Strings    ${TRACE_BASELINE}    ${found}
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.file.verbosity\=trace
    ${found}=    Get File    app.log
    Should Be Equal As Strings    ${TRACE_BASELINE}    ${found}

The applications prints a subset when setting the log level to DEBUG
    [Tags]    logging
    Remove Files    app.out    app.log
    LOG    "Print CAF log to stdout"
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.console.verbosity\=debug
    ...    stderr=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings    ${DEBUG_BASELINE}    ${found}
    LOG    "Print CAF log to a file"
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.file.verbosity\=debug
    ${found}=    Get File    app.log
    Should Be Equal As Strings    ${DEBUG_BASELINE}    ${found}

The applications prints a subset when setting the log level to INFO
    [Tags]    logging
    Remove Files    app.out    app.log
    LOG    "Print CAF log to stdout"
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.console.verbosity\=info
    ...    stderr=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings    ${INFO_BASELINE}    ${found}
    LOG    "Print CAF log to a file"
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.file.verbosity\=info
    ${found}=    Get File    app.log
    Should Be Equal As Strings    ${INFO_BASELINE}    ${found}

The applications prints a subset when setting the log level to WARN
    [Tags]    logging
    Remove Files    app.out    app.log
    LOG    "Print CAF log to stdout"
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.console.verbosity\=warning
    ...    stderr=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings    ${WARN_BASELINE}    ${found}
    LOG    "Print CAF log to a file"
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.file.verbosity\=warning
    ${found}=    Get File    app.log
    Should Be Equal As Strings    ${WARN_BASELINE}    ${found}

The applications prints a subset when setting the log level to ERROR
    [Tags]    logging
    Remove Files    app.out    app.log
    LOG    "Print CAF log to stdout"
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.console.verbosity\=error
    ...    stderr=app.out
    ${found}=    Get File    app.out
    Should Be Equal As Strings    ${ERROR_BASELINE}    ${found}
    LOG    "Print CAF log to a file"
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.file.verbosity\=error
    ${found}=    Get File    app.log
    Should Be Equal As Strings    ${ERROR_BASELINE}    ${found}

The applications logs nothing when setting the log level to QUIET
    [Tags]    logging
    Remove Files    app.out    app.log
    LOG    "Print CAF log to stdout"
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.console.verbosity\=quiet
    ...    stderr=app.out
    ${found}=    Get File    app.out
    Should Be Empty    ${found}
    LOG    "Print CAF log to a file"
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.file.verbosity\=quiet
    File Should Not Exist    app.log
