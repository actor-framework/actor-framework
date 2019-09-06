project(caf_io C CXX)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB_RECURSE LIBCAF_IO_HDRS "caf/*.hpp")

# list cpp files excluding platform-dependent files
set(LIBCAF_IO_SRCS
  src/abstract_broker.cpp
  src/acceptor.cpp
  src/acceptor_manager.cpp
  src/acceptor_manager.cpp
  src/basp_broker.cpp
  src/broker.cpp
  src/connection_helper.cpp
  src/datagram_handler.cpp
  src/datagram_manager.cpp
  src/datagram_servant.cpp
  src/datagram_servant_impl.cpp
  src/default_multiplexer.cpp
  src/doorman.cpp
  src/doorman_impl.cpp
  src/event_handler.cpp
  src/header.cpp
  src/instance.cpp
  src/interfaces.cpp
  src/ip_endpoint.cpp
  src/manager.cpp
  src/message_queue.cpp
  src/message_type.cpp
  src/middleman.cpp
  src/middleman_actor.cpp
  src/middleman_actor_impl.cpp
  src/multiplexer.cpp
  src/multiplexer.cpp
  src/native_socket.cpp
  src/pipe_reader.cpp
  src/protocol.cpp
  src/receive_buffer.cpp
  src/routing_table.cpp
  src/scribe.cpp
  src/scribe_impl.cpp
  src/socket_guard.cpp
  src/stream.cpp
  src/stream_manager.cpp
  src/tcp.cpp
  src/test_multiplexer.cpp
  src/udp.cpp
  src/worker.cpp
)

add_library(libcaf_io ${LIBCAF_IO_SRCS} ${LIBCAF_IO_HDRS})
target_link_libraries(libcaf_io PUBLIC caf::core)
target_include_directories(libcaf_io PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)
set_target_properties(libcaf_io
  PROPERTIES
  EXPORT_NAME io
  SOVERSION ${CAF_VERSION}
  VERSION ${CAF_VERSION}
  OUTPUT_NAME caf_io
)

install(TARGETS libcaf_io
  EXPORT CAFTargets
  ARCHIVE DESTINATION lib COMPONENT io
  RUNTIME DESTINATION bin COMPONENT io
  LIBRARY DESTINATION lib COMPONENT io
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION include
        COMPONENT io
        FILES_MATCHING PATTERN "*.hpp"
)

add_library(caf::io ALIAS libcaf_io)
if (BUILD_SHARED_LIBS)
  add_library(libcaf_io_shared ALIAS libcaf_io)
else ()
  add_library(libcaf_io_static ALIAS libcaf_io)
endif ()
list(APPEND CAF_LIBRARIES libcaf_io)
set(CAF_LIBRARIES ${CAF_LIBRARIES} PARENT_SCOPE)
