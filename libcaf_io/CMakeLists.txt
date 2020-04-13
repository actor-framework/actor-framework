# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE CAF_IO_HEADERS "caf/*.hpp")

# -- add consistency checks for enum to_string implementations -----------------

add_enum_consistency_check("caf/io/basp/message_type.hpp"
                           "src/io/basp/message_type_strings.cpp")

add_enum_consistency_check("caf/io/network/operation.hpp"
                           "src/io/network/operation_strings.cpp")

# -- add library target --------------------------------------------------------

add_library(libcaf_io_obj OBJECT ${CAF_IO_HEADERS}
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

target_include_directories(libcaf_io_obj PRIVATE
                           "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(libcaf_io
            "${PROJECT_SOURCE_DIR}/cmake/dummy.cpp"
            $<TARGET_OBJECTS:libcaf_io_obj>)

target_include_directories(libcaf_io INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

add_library(CAF::io ALIAS libcaf_io)

if(BUILD_SHARED_LIBS AND NOT WIN32)
  target_compile_options(libcaf_io PRIVATE -fPIC)
  target_compile_options(libcaf_io_obj PRIVATE -fPIC)
endif()

target_include_directories(libcaf_io_obj PRIVATE
  $<TARGET_PROPERTY:libcaf_core,INTERFACE_INCLUDE_DIRECTORIES>)

target_link_libraries(libcaf_io PUBLIC CAF::core)

if(MSVC)
  target_link_libraries(libcaf_io PUBLIC ws2_32 iphlpapi)
endif()

generate_export_header(libcaf_io
                       EXPORT_MACRO_NAME CAF_IO_EXPORT
                       EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/caf/detail/io_export.hpp"
                       STATIC_DEFINE CAF_STATIC_BUILD)

target_compile_definitions(libcaf_io_obj PRIVATE libcaf_io_EXPORTS)

set_target_properties(libcaf_io PROPERTIES
                      EXPORT_NAME io
                      SOVERSION ${CAF_VERSION}
                      VERSION ${CAF_LIB_VERSION}
                      OUTPUT_NAME caf_io)

# -- install library and header files ------------------------------------------

install(FILES "${CMAKE_BINARY_DIR}/caf/detail/io_export.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/caf/detail")

install(TARGETS libcaf_io
        EXPORT CAFTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT io
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT io
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT io)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT io
        FILES_MATCHING PATTERN "*.hpp")

# -- build unit tests ----------------------------------------------------------

if(NOT CAF_BUILD_TESTING)
  return()
endif()

add_executable(caf-io-test
               test/io-test.cpp
               $<TARGET_OBJECTS:libcaf_io_obj>)

target_include_directories(caf-io-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")

target_compile_definitions(caf-io-test PRIVATE libcaf_io_EXPORTS)

target_link_libraries(caf-io-test CAF::core CAF::test)

target_link_libraries(caf-core-test PRIVATE
  $<TARGET_PROPERTY:libcaf_io,INTERFACE_LINK_LIBRARIES>)

target_include_directories(caf-io-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

caf_add_test_suites(caf-io-test
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
  io.worker
)
