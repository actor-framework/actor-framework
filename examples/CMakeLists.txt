cmake_minimum_required(VERSION 2.8)
project(cppa_examples CXX)

add_custom_target(all_examples)

macro(add name folder)
    add_executable(${name} ${folder}/${name}.cpp ${ARGN})
    target_link_libraries(${name} ${CMAKE_DL_LIBS} ${CPPA_LIBRARY} ${PTHREAD_LIBRARIES})
    add_dependencies(${name} all_examples)
endmacro()

add(announce_1 type_system)
add(announce_2 type_system)
add(announce_3 type_system)
add(announce_4 type_system)
add(announce_5 type_system)
add(dancing_kirby message_passing)
add(dining_philosophers event_based_api)
add(hello_world .)
add(math_actor message_passing)
add(handling_posix_signals message_passing)
add(distributed_math_actor remote_actors)
add(group_server remote_actors)
add(group_chat remote_actors)

find_package(Qt4)

if (QT4_FOUND)
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
                        ${CPPA_LIBRARY}
                        ${QT_LIBRARIES})
  add_dependencies(qt_group_chat all_examples)
endif (QT4_FOUND)

# install (TARGETS announce_1 RUNTIME DESTINATION bin/)
# install (TARGETS announce_2 RUNTIME DESTINATION bin/)
# install (TARGETS announce_3 RUNTIME DESTINATION bin/)
# install (TARGETS announce_4 RUNTIME DESTINATION bin/)
# install (TARGETS announce_5 RUNTIME DESTINATION bin/)
# install (TARGETS dancing_kirby RUNTIME DESTINATION bin/)
# install (TARGETS dining_philosophers RUNTIME DESTINATION bin/)
# install (TARGETS hello_world RUNTIME DESTINATION bin/)
# install (TARGETS math_actor RUNTIME DESTINATION bin/)
# install (TARGETS distributed_math_actor RUNTIME DESTINATION bin/)
# install (TARGETS group_server RUNTIME DESTINATION bin/)
# install (TARGETS group_chat RUNTIME DESTINATION bin/)
# install (TARGETS handling_posix_signals RUNTIME DESTINATION bin/)






