if (DEFINED Python3_ROOT_DIR)
  set(Python_FIND_STRATEGY "LOCATION")
endif()

find_package(Python COMPONENTS Interpreter)

# -- tests for the CAF::core module --------------------------------------------

function(add_robot_test dir name)
  set(test_name caf-robot-${dir}-${name})
  set(test_wdir "${CMAKE_BINARY_DIR}/robot-output/${dir}/${name}")
  file(MAKE_DIRECTORY "${test_wdir}")
  add_test(
    NAME ${test_name}
    COMMAND
      ${Python_EXECUTABLE}
        -m robot
        ${ARGN}
        "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/${name}.robot"
    WORKING_DIRECTORY "${test_wdir}")
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 300)
endfunction()

execute_process(
  COMMAND ${Python_EXECUTABLE} -c "import robot"
  RESULT_VARIABLE EXIT_CODE
  OUTPUT_QUIET
)

if (NOT ${EXIT_CODE} EQUAL 0)
  message(FATAL_ERROR "Failed to import the 'robot' Python package")
endif()

caf_add_test_executable(
  caf-robot-config-options-driver
  SOURCES
    config/options-driver.cpp
  DEPENDENCIES
    CAF::core)

add_robot_test(
  config
  options
  --variable BINARY_PATH:$<TARGET_FILE:caf-robot-config-options-driver>
  --variable CONFIG_FILE_DIR:${CMAKE_CURRENT_SOURCE_DIR}/config)

add_robot_test(
  config
  dump-config
  --variable BINARY_PATH:$<TARGET_FILE:hello_world>)

caf_add_test_executable(
  caf-robot-flow-merge-driver
  SOURCES
    flow/merge-driver.cpp
  DEPENDENCIES
    CAF::core)

add_robot_test(
  flow
  merge
  --variable BINARY_PATH:$<TARGET_FILE:caf-robot-flow-merge-driver>)

# The logging test assumes that the CAF_LOG_LEVEL is set to TRACE.
if (CAF_LOG_LEVEL STREQUAL "TRACE")
  caf_add_test_executable(
    caf-robot-logging-driver
    SOURCES
      logging/driver.cpp
    DEPENDENCIES
      CAF::core)
  add_robot_test(
    logging
    output
    --variable BINARY_PATH:$<TARGET_FILE:caf-robot-logging-driver>)
endif()

# -- tests for the CAF::io module ----------------------------------------------
if(TARGET CAF::io)

  caf_add_test_executable(
    caf-robot-stream-driver
    SOURCES
      stream/driver.cpp
    DEPENDENCIES
      CAF::io)

  add_robot_test(
    stream
    remote
    --variable BINARY_PATH:$<TARGET_FILE:caf-robot-stream-driver>)

  caf_add_test_executable(
    caf-robot-middleman-driver
    SOURCES
      middleman/driver.cpp
    DEPENDENCIES
      CAF::io)

  add_robot_test(
    middleman
    middleman
    --variable BINARY_PATH:$<TARGET_FILE:caf-robot-middleman-driver>)

endif()

# -- tests for the CAF::net module ---------------------------------------------
if(TARGET CAF::net AND CAF_ENABLE_EXAMPLES)

  add_robot_test(
    web-socket
    quote-server
    --variable BINARY_PATH:$<TARGET_FILE:quote-server>
    --variable SSL_PATH:${CMAKE_CURRENT_SOURCE_DIR})

  add_robot_test(
    web-socket
    echo
    --variable BINARY_PATH:$<TARGET_FILE:echo>
    --variable SSL_PATH:${CMAKE_CURRENT_SOURCE_DIR})

  add_robot_test(
    http
    rest
    --variable BINARY_PATH:$<TARGET_FILE:rest>
    --variable SSL_PATH:${CMAKE_CURRENT_SOURCE_DIR})

  add_robot_test(
    http
    client
    --variable BINARY_PATH:$<TARGET_FILE:client>)

  # Temporary disabled: https://github.com/actor-framework/actor-framework/issues/1864
  # add_robot_test(
  #   lp
  #   communication
  #   --variable SERVER_PATH:$<TARGET_FILE:chat-server>
  #   --variable CLIENT_PATH:$<TARGET_FILE:chat-client>)

endif()
