# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE CAF_NET_HEADERS "caf/*.hpp")

# -- add targets ---------------------------------------------------------------

caf_incubator_add_component(
  net
  DEPENDENCIES
    PUBLIC
      CAF::core
      $<$<CXX_COMPILER_ID:MSVC>:ws2_32>
    PRIVATE
      CAF::internal
  ENUM_TYPES
    net.basp.connection_state
    net.basp.ec
    net.basp.message_type
    net.http.method
    net.http.status
    net.operation
    net.stream_transport_error
    net.web_socket.status
  HEADERS
    ${CAF_NET_HEADERS}
  SOURCES
    src/convert_ip_endpoint.cpp
    src/datagram_socket.cpp
    src/detail/rfc6455.cpp
    src/header.cpp
    src/host.cpp
    src/ip.cpp
    src/message_queue.cpp
    src/multiplexer.cpp
    src/net/abstract_actor_shell.cpp
    src/net/actor_shell.cpp
    src/net/http/header.cpp
    src/net/http/method.cpp
    src/net/http/status.cpp
    src/net/http/v1.cpp
    src/net/middleman.cpp
    src/net/middleman_backend.cpp
    src/net/packet_writer.cpp
    src/net/web_socket/handshake.cpp
    src/network_socket.cpp
    src/pipe_socket.cpp
    src/pollset_updater.cpp
    src/socket.cpp
    src/socket_manager.cpp
    src/stream_socket.cpp
    src/tcp_accept_socket.cpp
    src/tcp_stream_socket.cpp
    src/udp_datagram_socket.cpp
    src/worker.cpp
  TEST_SOURCES
    test/net-test.cpp
  TEST_SUITES
    accept_socket
    convert_ip_endpoint
    datagram_socket
    detail.rfc6455
    header
    ip
    multiplexer
    net.actor_shell
    net.consumer_adapter
    net.http.server
    net.length_prefix_framing
    net.operation
    net.producer_adapter
    net.typed_actor_shell
    net.web_socket.client
    net.web_socket.handshake
    net.web_socket.server
    network_socket
    pipe_socket
    socket
    socket_guard
    stream_socket
    stream_transport
    tcp_sockets
    udp_datagram_socket)

if(CAF_INC_ENABLE_TESTING AND TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
  caf_incubator_add_test_suites(caf-net-test net.openssl_transport)
  target_sources(caf-net-test PRIVATE test/net/openssl_transport_constants.cpp)
  target_link_libraries(caf-net-test PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()
