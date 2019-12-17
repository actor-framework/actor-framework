# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE CAF_NET_HEADERS "caf/*.hpp")

# -- auto generate to_string for enum types ------------------------------------

enum_to_string("caf/net/basp/connection_state.hpp" "basp_conn_strings.cpp")
enum_to_string("caf/net/basp/ec.hpp" "basp_ec_strings.cpp")
enum_to_string("caf/net/basp/message_type.hpp" "basp_message_type_strings.cpp")
enum_to_string("caf/net/operation.hpp" "operation_strings.cpp")

# -- list cpp files for caf::net -----------------------------------------------

set(CAF_NET_SOURCES
  "${CMAKE_BINARY_DIR}/basp_conn_strings.cpp"
  "${CMAKE_BINARY_DIR}/basp_ec_strings.cpp"
  "${CMAKE_BINARY_DIR}/basp_message_type_strings.cpp"
  "${CMAKE_BINARY_DIR}/operation_strings.cpp"
  src/actor_proxy_impl.cpp
  src/application.cpp
  src/convert_ip_endpoint.cpp
  src/datagram_socket.cpp
  src/defaults.cpp
  src/ec.cpp
  src/endpoint_manager.cpp
  src/header.cpp
  src/host.cpp
  src/ip.cpp
  src/multiplexer.cpp
  src/net/backend/test.cpp
  src/net/endpoint_manager_queue.cpp
  src/net/middleman.cpp
  src/net/middleman_backend.cpp
  src/net/packet_writer.cpp
  src/network_socket.cpp
  src/pipe_socket.cpp
  src/pollset_updater.cpp
  src/socket.cpp
  src/tcp_accept_socket.cpp
  src/tcp_stream_socket.cpp
  src/socket_manager.cpp
  src/stream_socket.cpp
  src/udp_datagram_socket.cpp
  src/defaults.cpp
  src/message_queue.cpp
  src/worker.cpp
)

# -- list cpp files for caf-net-test ------------------------------------------

set(CAF_NET_TEST_SOURCES
  test/net/basp/message_queue.cpp
  test/net/basp/ping_pong.cpp
  test/net/basp/worker.cpp
  test/accept_socket.cpp
  test/pipe_socket.cpp
  test/application.cpp
  test/socket.cpp
  test/convert_ip_endpoint.cpp
  test/socket_guard.cpp
  test/datagram_socket.cpp
  test/stream_application.cpp
  test/datagram_transport.cpp
  test/stream_socket.cpp
  test/doorman.cpp
  test/stream_transport.cpp
  test/endpoint_manager.cpp
  test/string_application.cpp
  test/header.cpp
  test/tcp_sockets.cpp
  test/ip.cpp
  test/transport_worker.cpp
  test/multiplexer.cpp
  test/transport_worker_dispatcher.cpp
  test/udp_datagram_socket.cpp
  test/network_socket.cpp
)

# -- add library target --------------------------------------------------------

add_library(libcaf_net_obj OBJECT ${CAF_NET_SOURCES} ${CAF_NET_HEADERS})

add_library(libcaf_net $<TARGET_OBJECTS:libcaf_net_obj>)

add_library(caf::net ALIAS libcaf_net)

if(BUILD_SHARED_LIBS AND NOT WIN32)
  target_compile_options(libcaf_net PRIVATE -fPIC)
  target_compile_options(libcaf_net_obj PRIVATE -fPIC)
endif()

target_link_libraries(libcaf_net PUBLIC ${CAF_EXTRA_LDFLAGS} ${CAF_LIBRARIES})

generate_export_header(libcaf_net
                       EXPORT_MACRO_NAME CAF_NET_EXPORT
                       EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/caf/detail/net_export.hpp"
                       STATIC_DEFINE CAF_STATIC_BUILD)

target_compile_definitions(libcaf_net_obj PRIVATE libcaf_net_EXPORTS)

set_target_properties(libcaf_net PROPERTIES
                      EXPORT_NAME net
                      SOVERSION ${CAF_VERSION}
                      VERSION ${CAF_LIB_VERSION}
                      OUTPUT_NAME caf_net)

# -- install library and header files ------------------------------------------

install(FILES "${CMAKE_BINARY_DIR}/caf/detail/net_export.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/caf/detail")

install(TARGETS libcaf_net
        EXPORT CAFTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT net
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT net
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT net)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT net
        FILES_MATCHING PATTERN "*.hpp")

# -- build unit tests ----------------------------------------------------------

if(NOT CAF_NO_UNIT_TESTS)
  add_executable(caf-net-test
                 "${PROJECT_SOURCE_DIR}/cmake/incubator-test.cpp"
                 "${CAF_INCLUDE_DIR_TEST}/caf/test/unit_test.hpp"
                 "${CAF_INCLUDE_DIR_TEST}/caf/test/unit_test_impl.hpp"
                 ${CAF_NET_TEST_SOURCES}
                 $<TARGET_OBJECTS:libcaf_net_obj>)
  target_compile_definitions(caf-net-test PRIVATE libcaf_net_EXPORTS)
  target_link_libraries(caf-net-test ${CAF_EXTRA_LDFLAGS} ${CAF_LIBRARIES})
  add_test_suites(caf-net-test
                  "${CMAKE_CURRENT_SOURCE_DIR}"
                  ${CAF_NET_TEST_SOURCES})
endif()

# -- add this library to the global CAF_LIBRARIES ------------------------------

list(APPEND CAF_LIBRARIES libcaf_net)

set(CAF_LIBRARIES ${CAF_LIBRARIES} PARENT_SCOPE)
