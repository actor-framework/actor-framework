# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE CAF_IO_HEADERS "caf/*.hpp")

# -- auto generate to_string for enum types ------------------------------------

enum_to_string("caf/io/basp/message_type.hpp" "message_type_to_string.cpp")
enum_to_string("caf/io/network/operation.hpp" "operation_to_string.cpp")

# -- list cpp files ------------------------------------------------------------

set(CAF_IO_SOURCES
  "${CMAKE_BINARY_DIR}/message_type_to_string.cpp"
  "${CMAKE_BINARY_DIR}/operation_to_string.cpp"
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

set(CAF_IO_TEST_SOURCES
  test/io/basp/message_queue.cpp
  test/io/basp_broker.cpp
  test/io/broker.cpp
  test/io/http_broker.cpp
  test/io/network/default_multiplexer.cpp
  test/io/network/ip_endpoint.cpp
  test/io/receive_buffer.cpp
  test/io/remote_actor.cpp
  test/io/remote_group.cpp
  test/io/remote_spawn.cpp
  test/io/unpublish.cpp
  test/io/worker.cpp
)

# -- add library target --------------------------------------------------------

add_library(libcaf_io_obj OBJECT ${CAF_IO_SOURCES} ${CAF_IO_HEADERS})

add_library(libcaf_io
            "${PROJECT_SOURCE_DIR}/cmake/dummy.cpp"
            $<TARGET_OBJECTS:libcaf_io_obj>)

add_library(caf::io ALIAS libcaf_io)

if(BUILD_SHARED_LIBS AND NOT WIN32)
  target_compile_options(libcaf_io PRIVATE -fPIC)
  target_compile_options(libcaf_io_obj PRIVATE -fPIC)
endif()

target_link_libraries(libcaf_io PUBLIC caf::core ${CAF_EXTRA_LDFLAGS})

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

if(NOT CAF_NO_UNIT_TESTS)
  add_executable(caf-io-test
                 "${PROJECT_SOURCE_DIR}/libcaf_test/src/caf-test.cpp"
                 "${PROJECT_SOURCE_DIR}/libcaf_test/caf/test/unit_test.hpp"
                 "${PROJECT_SOURCE_DIR}/libcaf_test/caf/test/unit_test_impl.hpp"
                 ${CAF_IO_TEST_SOURCES}
                 $<TARGET_OBJECTS:libcaf_io_obj>)
  target_compile_definitions(caf-io-test PRIVATE libcaf_io_EXPORTS)
  target_link_libraries(caf-io-test caf::core ${CAF_EXTRA_LDFLAGS})
  add_test_suites(caf-io-test
                  "${CMAKE_CURRENT_SOURCE_DIR}"
                  ${CAF_IO_TEST_SOURCES})
endif()

# -- add this library to the global CAF_LIBRARIES ------------------------------

list(APPEND CAF_LIBRARIES libcaf_io)

set(CAF_LIBRARIES ${CAF_LIBRARIES} PARENT_SCOPE)
