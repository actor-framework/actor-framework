project(caf_io C CXX)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB_RECURSE LIBCAF_IO_HDRS "caf/*.hpp")

enum_to_string("caf/io/basp/message_type.hpp" "message_type_to_string.cpp")
enum_to_string("caf/io/network/operation.hpp" "operation_to_string.cpp")

# list cpp files excluding platform-dependent files
set(LIBCAF_IO_SRCS
  "${CMAKE_CURRENT_BINARY_DIR}/message_type_to_string.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/operation_to_string.cpp"
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
  VERSION ${CAF_LIB_VERSION}
  OUTPUT_NAME caf_io
)

install(TARGETS libcaf_io
  EXPORT CAFTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT io
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT io
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT io
)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
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
