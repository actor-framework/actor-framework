if (DEFINED Python3_ROOT_DIR)
  set(Python_FIND_STRATEGY "LOCATION")
endif()

find_package(Python COMPONENTS Interpreter)

execute_process(
  COMMAND ${Python_EXECUTABLE} -c "import robot"
  RESULT_VARIABLE EXIT_CODE
  OUTPUT_QUIET
)

if (NOT ${EXIT_CODE} EQUAL 0)
  message(FATAL_ERROR "Failed to import the 'robot' Python package")
endif()

add_test(
  NAME "robot-config-read-config-file"
  COMMAND
    ${Python_EXECUTABLE}
      -m robot
      --variable BINARY_PATH:$<TARGET_FILE:hello_world>
      "${CMAKE_CURRENT_SOURCE_DIR}/config/read-config-file.robot")

add_test(
  NAME "robot-config-dump-config"
  COMMAND
    ${Python_EXECUTABLE}
      -m robot
      --variable BINARY_PATH:$<TARGET_FILE:hello_world>
      "${CMAKE_CURRENT_SOURCE_DIR}/config/dump-config.robot")

caf_add_test_executable(
  caf-robot-flow-merge-driver
  SOURCES
    flow/merge/driver.cpp
  DEPENDENCIES
    CAF::core)

add_test(
  NAME "caf-robot-flow-merge"
  COMMAND
    ${Python_EXECUTABLE}
      -m robot
      --variable BINARY_PATH:$<TARGET_FILE:caf-robot-flow-merge-driver>
      "${CMAKE_CURRENT_SOURCE_DIR}/flow/merge/merge.robot")

add_test(
  NAME "robot-http-rest"
  COMMAND
    ${Python_EXECUTABLE}
      -m robot
      --variable BINARY_PATH:$<TARGET_FILE:rest>
      --variable SSL_PATH:${CMAKE_CURRENT_SOURCE_DIR}
      "${CMAKE_CURRENT_SOURCE_DIR}/http/rest.robot")

# The logging test assumes that the CAF_LOG_LEVEL is set to TRACE.
if (CAF_LOG_LEVEL STREQUAL "TRACE")
  caf_add_test_executable(
    caf-robot-logging-driver
    SOURCES
      logging/driver.cpp
    DEPENDENCIES
      CAF::core)
  add_test(
    NAME "robot-logging-output"
    COMMAND
      ${Python_EXECUTABLE}
        -m robot
        --variable BINARY_PATH:$<TARGET_FILE:caf-robot-logging-driver>
        "${CMAKE_CURRENT_SOURCE_DIR}/logging/output.robot")
endif()

caf_add_test_executable(
  caf-robot-stream-driver
  SOURCES
    stream/driver.cpp
  DEPENDENCIES
    CAF::core
    CAF::io)

add_test(
  NAME "robot-stream-remote"
  COMMAND
    ${Python_EXECUTABLE}
      -m robot
      --variable BINARY_PATH:$<TARGET_FILE:caf-robot-stream-driver>
      "${CMAKE_CURRENT_SOURCE_DIR}/stream/remote.robot")

caf_add_test_executable(
  caf-robot-middleman-driver
  SOURCES
    middleman/driver.cpp
  DEPENDENCIES
    CAF::core
    CAF::io)

add_test(
  NAME "robot-middleman"
  COMMAND
    ${Python_EXECUTABLE}
      -m robot
      --variable BINARY_PATH:$<TARGET_FILE:caf-robot-middleman-driver>
      "${CMAKE_CURRENT_SOURCE_DIR}/middleman/middleman.robot")

add_test(
  NAME "robot-web-socket-quote-server"
  COMMAND
    ${Python_EXECUTABLE}
      -m robot
      --variable BINARY_PATH:$<TARGET_FILE:quote-server>
      --variable SSL_PATH:${CMAKE_CURRENT_SOURCE_DIR}
      "${CMAKE_CURRENT_SOURCE_DIR}/web_socket/quote-server.robot")

add_test(
  NAME "robot-web-socket-echo"
  COMMAND
    ${Python_EXECUTABLE}
      -m robot
      --variable BINARY_PATH:$<TARGET_FILE:echo>
      --variable SSL_PATH:${CMAKE_CURRENT_SOURCE_DIR}
      "${CMAKE_CURRENT_SOURCE_DIR}/web_socket/echo.robot")
