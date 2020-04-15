add_custom_target(all_examples)

function(add_example folder name)
  add_executable(${name} ${folder}/${name}.cpp ${ARGN})
  install(FILES ${folder}/${name}.cpp DESTINATION ${CMAKE_INSTALL_DATADIR}/caf/examples/${folder})
  add_dependencies(${name} all_examples)
endfunction()

function(add_core_example folder name)
  add_example(${folder} ${name} ${ARGN})
  target_link_libraries(${name} CAF::core)
endfunction()

# -- examples for CAF::core ----------------------------------------------------

# introductionary applications
add_core_example(. aout)
add_core_example(. hello_world)

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

# testing DSL
add_example(testing ping_pong)
target_link_libraries(ping_pong CAF::core CAF::test)


# -- examples for CAF::io ------------------------------------------------------

if(TARGET CAF::io)

  function(add_io_example folder name)
    add_example(${folder} ${name} ${ARGN})
    target_link_libraries(${name} CAF::io CAF::core)
  endfunction()

  # basic remoting
  add_io_example(remoting group_chat)
  add_io_example(remoting group_server)
  add_io_example(remoting remote_spawn)
  add_io_example(remoting distributed_calculator)

  # basic I/O with brokers
  add_io_example(broker simple_broker)
  add_io_example(broker simple_http_broker)

endif()


if(CAF_ENABLE_PROTOBUF_EXAMPLES)
  find_package(Protobuf REQUIRED)
  if(NOT PROTOBUF_PROTOC_EXECUTABLE)
    message(FATAL_ERROR "CMake was unable to set PROTOBUF_PROTOC_EXECUTABLE")
  endif()
  protobuf_generate_cpp(ProtoSources ProtoHeaders "${CMAKE_CURRENT_SOURCE_DIR}/remoting/pingpong.proto")
  include_directories(${PROTOBUF_INCLUDE_DIR})
  include_directories(${CMAKE_CURRENT_BINARY_DIR})
  add_executable(protobuf_broker broker/protobuf_broker.cpp ${ProtoSources})
  target_link_libraries(protobuf_broker ${PROTOBUF_LIBRARIES} CAF::core CAF::io)
  add_dependencies(protobuf_broker all_examples)
endif()

if(CAF_ENABLE_QT5_EXAMPLES)
  find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)
  message(STATUS "Found Qt5")
  #include(${QT_USE_FILE})
  QT5_ADD_RESOURCES(GROUP_CHAT_RCS )
  QT5_WRAP_UI(GROUP_CHAT_UI_HDR qtsupport/chatwindow.ui)
  QT5_WRAP_CPP(GROUP_CHAT_MOC_SRC qtsupport/chatwidget.hpp)
  # generated headers will be in cmake build directory
  include_directories(qtsupport
                      ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt5Core_INCLUDE_DIRS}
                      ${Qt5Gui_INCLUDE_DIRS}
                      ${Qt5Widgets_INCLUDE_DIRS})
  set(GROUP_CHAT_SRC qtsupport/qt_group_chat.cpp qtsupport/chatwidget.cpp)
  add_executable(qt_group_chat
                 ${GROUP_CHAT_SRC}
                 ${GROUP_CHAT_MOC_SRC}
                 ${GROUP_CHAT_UI_HDR})
  target_link_libraries(qt_group_chat
                        Qt5::Core
                        Qt5::Gui
                        Qt5::Widgets
                        CAF::core
                        CAF::io)
  add_dependencies(qt_group_chat all_examples)
endif()

if(CAF_ENABLE_CURL_EXAMPLES)
  find_package(CURL REQUIRED)
  add_executable(curl_fuse curl/curl_fuse.cpp)
  include_directories(${CURL_INCLUDE_DIRS})
  target_link_libraries(curl_fuse ${CURL_LIBRARY} CAF::core CAF::io)
  add_dependencies(curl_fuse all_examples)
endif()
