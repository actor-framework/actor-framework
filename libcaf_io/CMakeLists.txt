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
    src/detail/prometheus_broker.cpp
    src/detail/remote_group_module.cpp
    src/detail/socket_guard.cpp
    src/io/abstract_broker.cpp
    src/io/basp/header.cpp
    src/io/basp/instance.cpp
    src/io/basp/message_queue.cpp
    src/io/basp/routing_table.cpp
    src/io/basp/worker.cpp
    src/io/basp_broker.cpp
    src/io/broker.cpp
    src/io/connection_helper.cpp
    src/io/datagram_servant.cpp
    src/io/doorman.cpp
    src/io/middleman.cpp
    src/io/middleman_actor.cpp
    src/io/middleman_actor_impl.cpp
    src/io/network/acceptor.cpp
    src/io/network/acceptor_manager.cpp
    src/io/network/datagram_handler.cpp
    src/io/network/datagram_manager.cpp
    src/io/network/datagram_servant_impl.cpp
    src/io/network/default_multiplexer.cpp
    src/io/network/doorman_impl.cpp
    src/io/network/event_handler.cpp
    src/io/network/interfaces.cpp
    src/io/network/ip_endpoint.cpp
    src/io/network/manager.cpp
    src/io/network/multiplexer.cpp
    src/io/network/native_socket.cpp
    src/io/network/pipe_reader.cpp
    src/io/network/protocol.cpp
    src/io/network/receive_buffer.cpp
    src/io/network/scribe_impl.cpp
    src/io/network/stream.cpp
    src/io/network/stream_manager.cpp
    src/io/network/test_multiplexer.cpp
    src/io/scribe.cpp
    src/policy/tcp.cpp
    src/policy/udp.cpp
  TEST_SOURCES
    test/io-test.cpp
  TEST_SUITES
    detail.prometheus_broker
    io.basp.message_queue
    io.basp_broker
    io.broker
    io.http_broker
    io.monitor
    io.network.default_multiplexer
    io.network.ip_endpoint
    io.receive_buffer
    io.remote_actor
    io.remote_group
    io.remote_spawn
    io.unpublish
    io.worker)

if(CAF_ENABLE_TESTING AND UNIX)
  caf_add_test_suites(caf-io-test io.middleman)
endif()
