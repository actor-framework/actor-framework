cmake_minimum_required(VERSION 2.8)
project(caf_examples CXX)

add_custom_target(all_examples)

include_directories(${LIBCAF_INCLUDE_DIRS})

if(${CMAKE_SYSTEM_NAME} MATCHES "Window")
  set(WSLIB -lws2_32)
else ()
  set(WSLIB)
endif()

macro(add folder name)
  add_executable(${name} ${folder}/${name}.cpp ${ARGN})
  target_link_libraries(${name}
                        ${LD_FLAGS}
                        ${CAF_LIBRARIES}
                        ${PTHREAD_LIBRARIES}
                        ${WSLIB})
  install(FILES ${folder}/${name}.cpp DESTINATION share/caf/examples/${folder})
  add_dependencies(${name} all_examples)
endmacro()

# introductionary applications
add(. aout)
add(. hello_world)
add(. fibonacci)

# basic message passing primitives
add(message_passing cell)
add(message_passing divider)
add(message_passing request)
add(message_passing promises)
add(message_passing calculator)
add(message_passing delegating)
add(message_passing fixed_stack)
add(message_passing prioritizing)
add(message_passing dancing_kirby)

# dynamic behavior changes using 'become'
add(dynamic_behavior skip_messages)
add(dynamic_behavior dining_philosophers)

# composing actors using states or other actors
add(composition calculator_behavior)
add(composition dictionary_behavior)

# adding custom message types
add(custom_type custom_types_1)
add(custom_type custom_types_2)
add(custom_type custom_types_3)

# basic remoting
add(remoting group_chat)
add(remoting group_server)
add(remoting remote_spawn)
add(remoting distributed_calculator)

# basic I/O with brokers
add(broker simple_broker)
add(broker simple_http_broker)

if(NOT CAF_NO_PROTOBUF_EXAMPLES)
  find_package(Protobuf)
  if(PROTOBUF_FOUND AND PROTOBUF_PROTOC_EXECUTABLE)
    PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders "${CMAKE_CURRENT_SOURCE_DIR}/remoting/pingpong.proto")
    include_directories(${PROTOBUF_INCLUDE_DIR})
    # add binary dir as include path as generated headers will be located there
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    add_executable(protobuf_broker broker/protobuf_broker.cpp ${ProtoSources})
    target_link_libraries(protobuf_broker ${CMAKE_DL_LIBS} ${CAF_LIBRARIES} ${PTHREAD_LIBRARIES} ${PROTOBUF_LIBRARIES})
    add_dependencies(protobuf_broker all_examples)
  endif(PROTOBUF_FOUND AND PROTOBUF_PROTOC_EXECUTABLE)
endif()

if(NOT CAF_NO_QT_EXAMPLES)
  find_package(Qt5 COMPONENTS Core Gui Widgets QUIET)
  if(Qt5_FOUND)
    message(STATUS "Found Qt5")
    #include(${QT_USE_FILE})
    QT5_ADD_RESOURCES(GROUP_CHAT_RCS )
    QT5_WRAP_UI(GROUP_CHAT_UI_HDR qtsupport/chatwindow.ui)
    QT5_WRAP_CPP(GROUP_CHAT_MOC_SRC qtsupport/chatwidget.hpp)
    # generated headers will be in cmake build directory
    #include_directories(. qtsupport ${CMAKE_CURRENT_BINARY_DIR} ${CPPA_INCLUDE})
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
                          ${CMAKE_DL_LIBS}
                          ${CAF_LIBRARIES}
                          Qt5::Core
                          Qt5::Gui
                          Qt5::Widgets)
    add_dependencies(qt_group_chat all_examples)
  else()
    find_package(Qt4)
    if(QT4_FOUND)
      message(STATUS "Found Qt4")
      include(${QT_USE_FILE})
      QT4_ADD_RESOURCES(GROUP_CHAT_RCS )
      QT4_WRAP_UI(GROUP_CHAT_UI_HDR qtsupport/chatwindow.ui)
      QT4_WRAP_CPP(GROUP_CHAT_MOC_SRC qtsupport/chatwidget.hpp)
      # generated headers will be in cmake build directory
      #include_directories(. qtsupport ${CMAKE_CURRENT_BINARY_DIR} ${CPPA_INCLUDE})
      include_directories(qtsupport ${CMAKE_CURRENT_BINARY_DIR})
      set(GROUP_CHAT_SRCS qtsupport/qt_group_chat.cpp qtsupport/chatwidget.cpp)
      add_executable(qt_group_chat
                     ${GROUP_CHAT_SRCS}
                     ${GROUP_CHAT_MOC_SRC}
                     ${GROUP_CHAT_UI_HDR})
      target_link_libraries(qt_group_chat
                            ${CMAKE_DL_LIBS}
                            ${CAF_LIBRARIES}
                            ${QT_LIBRARIES})
      add_dependencies(qt_group_chat all_examples)
    endif()
  endif()
endif()

if(NOT CAF_NO_CURL_EXAMPLES)
  find_package(CURL)
  if(CURL_FOUND)
    add_executable(curl_fuse curl/curl_fuse.cpp)
    include_directories(${CURL_INCLUDE_DIRS})
    target_link_libraries(curl_fuse ${CMAKE_DL_LIBS} ${CAF_LIBRARIES} ${PTHREAD_LIBRARIES} ${CURL_LIBRARY})
    add_dependencies(curl_fuse all_examples)
  endif(CURL_FOUND)
endif()
