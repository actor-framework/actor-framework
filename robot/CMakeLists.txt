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

add_test(
  NAME "robot-http-rest"
  COMMAND
    ${Python_EXECUTABLE}
      -m robot
      --variable BINARY_PATH:$<TARGET_FILE:rest>
      --variable SSL_PATH:${CMAKE_CURRENT_SOURCE_DIR}
      "${CMAKE_CURRENT_SOURCE_DIR}/http/rest.robot")

add_test(
  NAME "robot-logging-output"
  COMMAND
    ${Python_EXECUTABLE}
      -m robot
      --variable BINARY_PATH:$<TARGET_FILE:caf-core-logging-driver>
      "${CMAKE_CURRENT_SOURCE_DIR}/logging/output.robot")

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
