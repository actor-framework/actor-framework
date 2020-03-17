# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB_RECURSE LIBCAF_IO_HDRS "caf/*.hpp")

add_enum_consistency_check("caf/io/basp/message_type.hpp"
                           "src/io/basp/message_type_strings.cpp")

add_enum_consistency_check("caf/io/network/operation.hpp"
                           "src/io/network/operation_strings.cpp")

# list cpp files excluding platform-dependent files
set(LIBCAF_IO_SRCS
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

add_custom_target(libcaf_io)

# build shared library if not compiling static only
if (NOT CAF_BUILD_STATIC_ONLY)
  add_library(libcaf_io_shared SHARED ${LIBCAF_IO_SRCS} ${LIBCAF_IO_HDRS})
  target_link_libraries(libcaf_io_shared ${CAF_EXTRA_LDFLAGS} ${CAF_LIBRARY_CORE})
  target_include_directories(libcaf_io_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  set_target_properties(libcaf_io_shared
                        PROPERTIES
                        SOVERSION ${CAF_VERSION}
                        VERSION ${CAF_LIB_VERSION}
                        OUTPUT_NAME caf_io)
  install(TARGETS libcaf_io_shared
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  add_dependencies(libcaf_io_shared libcaf_io)
endif ()

# build static library only if --build-static or --build-static-only was set
if (CAF_BUILD_STATIC_ONLY OR CAF_BUILD_STATIC)
  add_library(libcaf_io_static STATIC ${LIBCAF_IO_HDRS} ${LIBCAF_IO_SRCS})
  target_link_libraries(libcaf_io_static ${CAF_EXTRA_LDFLAGS} ${CAF_LIBRARY_CORE_STATIC})
  target_include_directories(libcaf_io_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  set_target_properties(libcaf_io_static PROPERTIES OUTPUT_NAME caf_io_static)
  install(TARGETS libcaf_io_static ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  add_dependencies(libcaf_io_static libcaf_io)
endif ()

# install includes
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/caf
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp")
