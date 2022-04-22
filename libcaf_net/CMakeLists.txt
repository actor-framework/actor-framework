# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE CAF_NET_HEADERS "caf/*.hpp")

# -- add targets ---------------------------------------------------------------

caf_add_component(
  net
  DEPENDENCIES
    PUBLIC
      CAF::core
      $<$<CXX_COMPILER_ID:MSVC>:ws2_32>
    PRIVATE
      CAF::internal
  ENUM_TYPES
    net.http.method
    net.http.status
    net.operation
    net.stream_transport_error
    net.web_socket.status
  HEADERS
    ${CAF_NET_HEADERS}
  SOURCES
    src/datagram_socket.cpp
    src/detail/convert_ip_endpoint.cpp
    src/detail/rfc6455.cpp
    src/host.cpp
    src/ip.cpp
    src/multiplexer.cpp
    src/net/abstract_actor_shell.cpp
    src/net/actor_shell.cpp
    src/net/http/header.cpp
    src/net/http/method.cpp
    src/net/http/status.cpp
    src/net/http/v1.cpp
    src/net/middleman.cpp
    src/net/web_socket/default_trait.cpp
    src/net/web_socket/frame.cpp
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
  TEST_SOURCES
    test/net-test.cpp
  TEST_SUITES
    detail.convert_ip_endpoint
    detail.rfc6455
    net.accept_socket
    net.actor_shell
    net.consumer_adapter
    net.datagram_socket
    net.http.server
    net.ip
    net.length_prefix_framing
    net.multiplexer
    net.network_socket
    net.operation
    net.pipe_socket
    net.producer_adapter
    net.socket
    net.socket_guard
    net.stream_socket
    net.stream_transport
    net.tcp_socket
    net.typed_actor_shell
    net.udp_datagram_socket
    net.web_socket.client
    net.web_socket.handshake
    net.web_socket.server)

if(CAF_INC_ENABLE_TESTING AND TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
  caf_incubator_add_test_suites(caf-net-test net.openssl_transport)
  target_sources(caf-net-test PRIVATE test/net/openssl_transport_constants.cpp)
  target_link_libraries(caf-net-test PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()
