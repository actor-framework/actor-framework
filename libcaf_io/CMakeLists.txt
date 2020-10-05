# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE CAF_IO_HEADERS "caf/*.hpp")

# -- add consistency checks for enum to_string implementations -----------------

caf_add_enum_consistency_check("caf/io/basp/message_type.hpp"
                               "src/io/basp/message_type_strings.cpp")

caf_add_enum_consistency_check("caf/io/network/operation.hpp"
                               "src/io/network/operation_strings.cpp")

# -- utility function for setting default properties ---------------------------

function(caf_io_set_default_properties)
  foreach(target ${ARGN})
    # Set global defaults and set EXPORTS flag.
    caf_set_default_properties(${target})
    target_compile_definitions(${target} PRIVATE libcaf_io_EXPORTS)
    # Pull in public dependencies.
    caf_target_link_libraries(${target} PUBLIC CAF::core)
    if(MSVC)
      caf_target_link_libraries(${target} PUBLIC ws2_32 iphlpapi)
    endif()
  endforeach()
endfunction()

# -- add library target --------------------------------------------------------

add_library(libcaf_io_obj OBJECT ${CAF_IO_HEADERS}
  src/detail/prometheus_broker.cpp
  src/detail/socket_guard.cpp
  src/io/abstract_broker.cpp
  src/io/basp/header.cpp
  src/io/basp/instance.cpp
  src/io/basp/message_queue.cpp
  src/io/basp/message_type_strings.cpp
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
  src/io/network/operation_strings.cpp
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
)

add_library(libcaf_io
            "${PROJECT_SOURCE_DIR}/cmake/dummy.cpp"
            $<TARGET_OBJECTS:libcaf_io_obj>)

caf_io_set_default_properties(libcaf_io_obj libcaf_io)

caf_export_and_install_lib(io)

# -- build unit tests ----------------------------------------------------------

if(NOT CAF_ENABLE_TESTING)
  return()
endif()

add_executable(caf-io-test
               test/io-test.cpp
               $<TARGET_OBJECTS:libcaf_io_obj>)

caf_io_set_default_properties(caf-io-test)

target_include_directories(caf-io-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")

target_link_libraries(caf-io-test PRIVATE CAF::test)

caf_add_test_suites(caf-io-test
  detail.prometheus_broker
  io.basp.message_queue
  io.basp_broker
  io.broker
  io.http_broker
  $<$<NOT:$<PLATFORM_ID:Windows>>:io.middleman>
  io.monitor
  io.network.default_multiplexer
  io.network.ip_endpoint
  io.receive_buffer
  io.remote_actor
  io.remote_group
  io.remote_spawn
  io.unpublish
  io.worker
)
