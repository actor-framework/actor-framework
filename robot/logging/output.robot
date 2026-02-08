*** Settings ***
Documentation       A test suite for --dump-config.

Library             OperatingSystem
Library             Process
Library             String


*** Variables ***
${BINARY_PATH}          /path/to/the/test/binary

${TRACE_BASELINE}=      SEPARATOR=
...                     TRACE app ENTRY value = 42\n
...                     DEBUG app this is a debug message\n
...                     DEBUG app this is another debug message with foobar("one", "two") ; field = foobar("three", "four")\n
...                     INFO app this is an info message\n
...                     INFO app this is another info message ; foo = bar\n
...                     WARN app this is a warning message\n
...                     WARN app this is another warning message ; foo = bar\n
...                     ERROR app this is an error message\n
...                     ERROR app this is another error message ; foo = bar\n
...                     TRACE app EXIT\n

${DEBUG_BASELINE}=      SEPARATOR=
...                     DEBUG app this is a debug message\n
...                     DEBUG app this is another debug message with foobar("one", "two") ; field = foobar("three", "four")\n
...                     INFO app this is an info message\n
...                     INFO app this is another info message ; foo = bar\n
...                     WARN app this is a warning message\n
...                     WARN app this is another warning message ; foo = bar\n
...                     ERROR app this is an error message\n
...                     ERROR app this is another error message ; foo = bar\n

${INFO_BASELINE}=       SEPARATOR=
...                     INFO app this is an info message\n
...                     INFO app this is another info message ; foo = bar\n
...                     WARN app this is a warning message\n
...                     WARN app this is another warning message ; foo = bar\n
...                     ERROR app this is an error message\n
...                     ERROR app this is another error message ; foo = bar\n

${WARN_BASELINE}=       SEPARATOR=
...                     WARN app this is a warning message\n
...                     WARN app this is another warning message ; foo = bar\n
...                     ERROR app this is an error message\n
...                     ERROR app this is another error message ; foo = bar\n

${ERROR_BASELINE}=      SEPARATOR=
...                     ERROR app this is an error message\n
...                     ERROR app this is another error message ; foo = bar\n

${ALL_EXCLUDED_COMPONENT}=  ["app", "caf", "caf_flow", "caf.core", "caf.system"]

${COMMON_EXCLUDED_COMPONENT}=   ["caf", "caf_flow", "caf.core", "caf.system"]


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
    RETURN        ${output}

Run File Logger
    [Arguments]    ${verbosity}   ${api}
    Remove File    app.log
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.file.verbosity\=${verbosity}
    ...    --api\=${api}

Run Console And File Logger
    [Arguments]    ${console_excluded_components}   ${file_excluded_components}
    Remove File    app.out
    Remove File    app.log
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.console.verbosity\=debug
    ...    --caf.logger.file.verbosity\=debug
    ...    --caf.logger.console.excluded-components\=${console_excluded_components}
    ...    --caf.logger.file.excluded-components\=${file_excluded_components}
    ...    stderr=app.out
    ${console_output}=    Get File    app.out
    TRY
        ${file_output}=    Get File    app.log
    EXCEPT
        ${file_output}=    Set Variable    ${EMPTY}
    END
    RETURN        ${console_output}     ${file_output}

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

The logger rejects file and console logging for excluded components
    [Tags]    logging
    ${console}    ${file}=    Run Console And File Logger    ${ALL_EXCLUDED_COMPONENT}    ${ALL_EXCLUDED_COMPONENT}
    Should Be Empty    ${console}
    Should Be Empty    ${file}

The logger rejects file logging for excluded components
    [Tags]    logging
    ${console}    ${file}=    Run Console And File Logger    ${COMMON_EXCLUDED_COMPONENT}    ${ALL_EXCLUDED_COMPONENT}
    Should Be Equal As Strings    ${DEBUG_BASELINE}    ${console}    formatter=repr
    Should Be Empty    ${file}

The logger rejects console logging for excluded components
    [Tags]    logging
    ${console}    ${file}=    Run Console And File Logger    ${ALL_EXCLUDED_COMPONENT}    ${COMMON_EXCLUDED_COMPONENT}
    Should Be Empty    ${console}
    Should Be Equal As Strings    ${DEBUG_BASELINE}    ${file}    formatter=repr

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

The file logger renders formatted messages with fields
    [Tags]    logging
    Remove File    app.log
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.file.verbosity\=debug
    ...    --caf.logger.file.format\=%p %m%n
    ...    --test\=formatted
    ${found}=    Get File    app.log
    Should Contain    ${found}    formatted message: foo + bar = foobar
    Should Contain    ${found}    value = 42
    Should Contain    ${found}    key = value
    Should Contain    ${found}    n = 123

The file logger includes thread id and runtime in formatted output
    [Tags]    logging
    Remove File    app.log
    Run Process    ${BINARY_PATH}
    ...    --config-file\=${CURDIR}${/}base.cfg
    ...    --caf.logger.file.verbosity\=debug
    ...    --caf.logger.file.format\=%r %t %m%n
    ...    --test\=thread_runtime
    ${found}=    Get File    app.log
    Should Match Regexp    ${found}    \\d+\\s+\\d+\\s+thread_runtime_check
