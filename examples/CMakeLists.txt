add_custom_target(all_examples)

function(add_example folder name)
  add_executable(${name} ${folder}/${name}.cpp ${ARGN})
  set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                        "${CMAKE_CURRENT_BINARY_DIR}/${folder}")
  install(FILES ${folder}/${name}.cpp
          DESTINATION ${CMAKE_INSTALL_DATADIR}/caf/examples/${folder})
  add_dependencies(${name} all_examples)
endfunction()

function(add_core_example folder name)
  add_example(${folder} ${name} ${ARGN})
  target_link_libraries(${name} PRIVATE CAF::internal CAF::core)
endfunction()

# -- examples for CAF::core ----------------------------------------------------

# introductory applications
add_core_example(. hello_world)
add_core_example(. println)

# configuration API
add_core_example(config read-json)

# basic message passing primitives
add_core_example(message_passing calculator)
add_core_example(message_passing dancing_kirby)
add_core_example(message_passing delegating)
add_core_example(message_passing divider)
add_core_example(message_passing fan_out_request)
add_core_example(message_passing idle_timeout_once)
add_core_example(message_passing idle_timeout_repeat)
add_core_example(message_passing promises)
add_core_example(message_passing request)

# flow API
add_core_example(flow iota)
add_core_example(flow observe-on)
add_core_example(flow spsc-buffer-resource)

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
  add_io_example(remoting remote_spawn)
  add_io_example(remoting distributed_calculator)

  if(CAF_ENABLE_CURL_EXAMPLES)
    find_package(CURL REQUIRED)
    add_executable(curl_fuse curl/curl_fuse.cpp)
    include_directories(${CURL_INCLUDE_DIRS})
    target_link_libraries(curl_fuse ${CURL_LIBRARY} CAF::io CAF::internal)
    add_dependencies(curl_fuse all_examples)
  endif()

endif()

if(TARGET CAF::net)
  function(add_net_example folder name)
    add_example(${folder} ${name} ${ARGN})
    target_link_libraries(${name} PRIVATE CAF::internal CAF::net)
  endfunction()
else()
  function(add_net_example)
    # nop
  endfunction()
endif()

add_net_example(http client)
add_net_example(http rest)
add_net_example(http time-server)

add_net_example(length_prefix_framing chat-client)
add_net_example(length_prefix_framing chat-server)

add_net_example(web_socket echo)
add_net_example(web_socket hello-client)
add_net_example(web_socket quote-server)
add_net_example(web_socket stock-ticker)

if(TARGET CAF::net AND CAF_ENABLE_QT6_EXAMPLES)
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
                        CAF::net
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
