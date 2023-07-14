# -- collect header files ------------------------------------------------------

file(GLOB_RECURSE CAF_IO_HEADERS "caf/*.hpp")

# -- add targets ---------------------------------------------------------------

caf_add_component(
  io
  DEPENDENCIES
    PUBLIC
      CAF::core
      $<$<CXX_COMPILER_ID:MSVC>:ws2_32>
    PRIVATE
      CAF::internal
  ENUM_TYPES
    io.basp.message_type
    io.network.operation
  HEADERS
    ${CAF_IO_HEADERS}
  SOURCES
    caf/detail/prometheus_broker.cpp
    caf/detail/remote_group_module.cpp
    caf/detail/socket_guard.cpp
    caf/io/abstract_broker.cpp
    caf/io/basp/header.cpp
    caf/io/basp/instance.cpp
    caf/io/basp/message_queue.cpp
    caf/io/basp/routing_table.cpp
    caf/io/basp/worker.cpp
    caf/io/basp_broker.cpp
    caf/io/broker.cpp
    caf/io/connection_helper.cpp
    caf/io/datagram_servant.cpp
    caf/io/doorman.cpp
    caf/io/middleman.cpp
    caf/io/middleman_actor.cpp
    caf/io/middleman_actor_impl.cpp
    caf/io/network/acceptor.cpp
    caf/io/network/acceptor_manager.cpp
    caf/io/network/datagram_handler.cpp
    caf/io/network/datagram_manager.cpp
    caf/io/network/datagram_servant_impl.cpp
    caf/io/network/default_multiplexer.cpp
    caf/io/network/doorman_impl.cpp
    caf/io/network/event_handler.cpp
    caf/io/network/interfaces.cpp
    caf/io/network/ip_endpoint.cpp
    caf/io/network/manager.cpp
    caf/io/network/multiplexer.cpp
    caf/io/network/native_socket.cpp
    caf/io/network/pipe_reader.cpp
    caf/io/network/protocol.cpp
    caf/io/network/receive_buffer.cpp
    caf/io/network/receive_buffer.test.cpp
    caf/io/network/scribe_impl.cpp
    caf/io/network/stream.cpp
    caf/io/network/stream_manager.cpp
    caf/io/network/test_multiplexer.cpp
    caf/io/scribe.cpp
    caf/policy/tcp.cpp
    caf/policy/udp.cpp
  LEGACY_TEST_SOURCES
    test/io-test.cpp
  LEGACY_TEST_SUITES
    detail.prometheus_broker
    io.basp.message_queue
    io.basp_broker
    io.broker
    io.http_broker
    io.monitor
    io.network.default_multiplexer
    io.network.ip_endpoint
    io.remote_actor
    io.remote_group
    io.remote_spawn
    io.unpublish
    io.worker)

if(CAF_ENABLE_TESTING AND UNIX)
  caf_add_test_suites(caf-io-legacy-test io.middleman)
endif()
