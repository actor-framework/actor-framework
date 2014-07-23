cmake_minimum_required(VERSION 2.8)
project(caf_examples CXX)

add_custom_target(all_examples)

include_directories(${LIBCAF_INCLUDE_DIRS})

if (${CMAKE_SYSTEM_NAME} MATCHES "Window")
  set(WSLIB -lws2_32)
else ()
  set(WSLIB)
endif ()

macro(add name folder)
  add_executable(${name} ${folder}/${name}.cpp ${ARGN})
  target_link_libraries(${name}
                        ${CMAKE_DL_LIBS}
			${LIBCAF_LIBRARIES}
                        ${PTHREAD_LIBRARIES}
                        ${WSLIB})
  add_dependencies(${name} all_examples)
endmacro()

add(announce_1 type_system)
add(announce_2 type_system)
add(announce_3 type_system)
add(announce_4 type_system)
add(announce_5 type_system)
add(dancing_kirby message_passing)
add(dining_philosophers message_passing)
add(hello_world .)
add(aout .)
add(calculator message_passing)
add(typed_calculator message_passing)
add(distributed_calculator remote_actors)
add(group_server remote_actors)
add(group_chat remote_actors)
add(simple_broker remote_actors)

if (NOT "${CAF_NO_PROTOBUF_EXAMPLES}" STREQUAL "yes")
  find_package(Protobuf)
  if (PROTOBUF_FOUND)
    PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders "${CMAKE_CURRENT_SOURCE_DIR}/remote_actors/pingpong.proto")
    include_directories(${PROTOBUF_INCLUDE_DIR})
    # add binary dir as include path as generated headers will be located there
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    add_executable(protobuf_broker remote_actors/protobuf_broker.cpp ${ProtoSources})
    target_link_libraries(protobuf_broker ${CMAKE_DL_LIBS} ${CAF_LIBRARY} ${PTHREAD_LIBRARIES} ${PROTOBUF_LIBRARIES})
    add_dependencies(protobuf_broker all_examples)
  endif (PROTOBUF_FOUND)
endif ()

if (NOT "${CAF_NO_CURL_EXAMPLES}" STREQUAL "yes")
  find_package(CURL)
  if (CURL_FOUND)
    add_executable(curl_fuse curl/curl_fuse.cpp)
    target_link_libraries(curl_fuse ${CMAKE_DL_LIBS} ${LIBCAF_LIBRARIES} ${PTHREAD_LIBRARIES} ${CURL_LIBRARY})
    add_dependencies(curl_fuse all_examples)
  endif (CURL_FOUND)
endif ()
