cmake_minimum_required(VERSION 2.8.12)
project(caf_io C CXX)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB_RECURSE LIBCAF_IO_HDRS "caf/*.hpp")

# list cpp files excluding platform-dependent files
set(LIBCAF_IO_SRCS
  src/abstract_broker.cpp
  src/acceptor_manager.cpp
  src/acceptor_manager.cpp
  src/basp_broker.cpp
  src/broker.cpp
  src/connection_helper.cpp
  src/datagram_manager.cpp
  src/datagram_servant.cpp
  src/default_multiplexer.cpp
  src/doorman.cpp
  src/header.cpp
  src/hook.cpp
  src/instance.cpp
  src/interfaces.cpp
  src/ip_endpoint.cpp
  src/manager.cpp
  src/message_type.cpp
  src/middleman.cpp
  src/middleman_actor.cpp
  src/middleman_actor_impl.cpp
  src/multiplexer.cpp
  src/multiplexer.cpp
  src/protocol.cpp
  src/receive_buffer.cpp
  src/routing_table.cpp
  src/scribe.cpp
  src/stream_manager.cpp
  src/test_multiplexer.cpp
  src/acceptor.cpp
  src/datagram_handler.cpp
  src/datagram_servant_impl.cpp
  src/doorman_impl.cpp
  src/event_handler.cpp
  src/pipe_reader.cpp
  src/scribe_impl.cpp
  src/stream.cpp
  src/tcp.cpp
  src/udp.cpp
  src/native_socket.cpp
  src/socket_guard.cpp
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
                        VERSION ${CAF_VERSION}
                        OUTPUT_NAME caf_io)
  install(TARGETS libcaf_io_shared
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION lib)
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
  install(TARGETS libcaf_io_static ARCHIVE DESTINATION lib)
  add_dependencies(libcaf_io_static libcaf_io)
endif ()

# install includes
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/caf
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp")
