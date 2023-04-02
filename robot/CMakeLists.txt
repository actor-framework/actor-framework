find_package(Python COMPONENTS Interpreter)

add_test(
  NAME "robot-http-rest"
  COMMAND
    ${Python_EXECUTABLE}
      -m robot
      --variable BINARY_PATH:$<TARGET_FILE:rest>
      --variable SSL_PATH:${CMAKE_CURRENT_SOURCE_DIR}
      "${CMAKE_CURRENT_SOURCE_DIR}/http/rest.robot")
