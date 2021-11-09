add_custom_target(all_examples)

function(add_example folder name)
  add_executable(${name} ${folder}/${name}.cpp ${ARGN})
  install(FILES ${folder}/${name}.cpp
          DESTINATION ${CMAKE_INSTALL_DATADIR}/caf/examples/${folder})
  add_dependencies(${name} all_examples)
endfunction()

function(add_core_example folder name)
  add_example(${folder} ${name} ${ARGN})
  target_link_libraries(${name} PRIVATE CAF::internal CAF::core)
endfunction()

# -- examples for CAF::core ----------------------------------------------------

# introductionary applications
add_core_example(. aout)
add_core_example(. hello_world)

# configuration API
add_core_example(config read-json)

# basic message passing primitives
add_core_example(message_passing calculator)
add_core_example(message_passing cell)
add_core_example(message_passing dancing_kirby)
add_core_example(message_passing delegating)
add_core_example(message_passing divider)
add_core_example(message_passing fan_out_request)
add_core_example(message_passing fixed_stack)
add_core_example(message_passing promises)
add_core_example(message_passing request)
add_core_example(message_passing typed_calculator)

# streaming API
add_core_example(streaming integer_stream)

# dynamic behavior changes using 'become'
add_core_example(dynamic_behavior skip_messages)
add_core_example(dynamic_behavior dining_philosophers)

# adding custom message types
add_core_example(custom_type custom_types_1)
add_core_example(custom_type custom_types_2)
add_core_example(custom_type custom_types_3)
add_core_example(custom_type custom_types_4)

# testing DSL
add_example(testing ping_pong)
target_link_libraries(ping_pong PRIVATE CAF::internal CAF::core CAF::test)
add_test(NAME "examples.ping-pong" COMMAND ping_pong -r300 -n -v5)

# -- examples for CAF::io ------------------------------------------------------

if(TARGET CAF::io)

  function(add_io_example folder name)
    add_example(${folder} ${name} ${ARGN})
    target_link_libraries(${name} PRIVATE CAF::internal CAF::io)
  endfunction()

  # basic remoting
  add_io_example(remoting group_chat)
  add_io_example(remoting group_server)
  add_io_example(remoting remote_spawn)
  add_io_example(remoting distributed_calculator)

  # basic I/O with brokers
  add_io_example(broker simple_broker)
  add_io_example(broker simple_http_broker)

  if(CAF_ENABLE_PROTOBUF_EXAMPLES)
    find_package(Protobuf REQUIRED)
    if(NOT PROTOBUF_PROTOC_EXECUTABLE)
      message(FATAL_ERROR "CMake was unable to set PROTOBUF_PROTOC_EXECUTABLE")
    endif()
    protobuf_generate_cpp(ProtoSources ProtoHeaders "${CMAKE_CURRENT_SOURCE_DIR}/remoting/pingpong.proto")
    include_directories(${PROTOBUF_INCLUDE_DIR})
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    add_executable(protobuf_broker broker/protobuf_broker.cpp ${ProtoSources})
    target_link_libraries(protobuf_broker
                          PRIVATE ${PROTOBUF_LIBRARIES} CAF::internal CAF::io)
    add_dependencies(protobuf_broker all_examples)
  endif()

  if(CAF_ENABLE_QT6_EXAMPLES)
    find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
    qt6_wrap_ui(GROUP_CHAT_UI_HDR qtsupport/chatwindow.ui)
    qt6_wrap_cpp(GROUP_CHAT_MOC_SRC qtsupport/chatwidget.hpp)
    # generated headers will be in cmake build directory
    add_executable(qt_group_chat
                   qtsupport/qt_group_chat.cpp
                   qtsupport/chatwidget.cpp
                   ${GROUP_CHAT_MOC_SRC}
                   ${GROUP_CHAT_UI_HDR})
    target_link_libraries(qt_group_chat
                          CAF::io
                          CAF::internal
                          Qt6::Core
                          Qt6::Gui
                          Qt6::Widgets)
    target_include_directories(qt_group_chat PRIVATE
                               qtsupport
                               ${CMAKE_CURRENT_BINARY_DIR}
                               ${Qt6Core_INCLUDE_DIRS}
                               ${Qt6Gui_INCLUDE_DIRS}
                               ${Qt6Widgets_INCLUDE_DIRS})
    if (CMAKE_VERSION VERSION_LESS 3.8)
      message(STATUS "Note: build fails if Qt6 sets incompatible -std=ARG flag")
    else()
      set_property(TARGET qt_group_chat PROPERTY CXX_STANDARD 17)
    endif()
    add_dependencies(qt_group_chat all_examples)
  endif()

  if(CAF_ENABLE_CURL_EXAMPLES)
    find_package(CURL REQUIRED)
    add_executable(curl_fuse curl/curl_fuse.cpp)
    include_directories(${CURL_INCLUDE_DIRS})
    target_link_libraries(curl_fuse ${CURL_LIBRARY} CAF::io CAF::internal)
    add_dependencies(curl_fuse all_examples)
  endif()

endif()
