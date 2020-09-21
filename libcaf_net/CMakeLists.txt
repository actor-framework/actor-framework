# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE CAF_NET_HEADERS "caf/*.hpp")

# -- add consistency checks for enum to_string implementations -----------------

caf_incubator_add_enum_consistency_check("caf/net/basp/connection_state.hpp"
                                         "src/basp/connection_state_strings.cpp")
caf_incubator_add_enum_consistency_check("caf/net/basp/ec.hpp"
                                         "src/basp/ec_strings.cpp")
caf_incubator_add_enum_consistency_check("caf/net/basp/message_type.hpp"
                                         "src/basp/message_type_strings.cpp")
caf_incubator_add_enum_consistency_check("caf/net/operation.hpp"
                                         "src/basp/operation_strings.cpp")

# -- utility function for setting default properties ---------------------------

function(caf_net_set_default_properties)
  foreach(target ${ARGN})
    caf_incubator_set_default_properties(${target})
    # Make sure we find our headers plus the the generated export header.
    target_include_directories(${target} PRIVATE
                               "${CMAKE_CURRENT_SOURCE_DIR}"
                               "${CMAKE_BINARY_DIR}")
    target_compile_definitions(${target} PRIVATE libcaf_net_EXPORTS)
    # Pull in public dependencies.
    target_link_libraries(${target} PUBLIC CAF::core)
    if(MSVC)
      target_link_libraries(${target} PUBLIC ws2_32 iphlpapi)
    endif()
  endforeach()
endfunction()

# -- add library targets -------------------------------------------------------

add_library(libcaf_net_obj OBJECT ${CAF_NET_HEADERS}
  #src/net/backend/tcp.cpp
  #src/net/backend/test.cpp
  #src/actor_proxy_impl.cpp
  #src/basp/application.cpp
  src/basp/connection_state_strings.cpp
  src/basp/ec_strings.cpp
  src/basp/message_type_strings.cpp
  src/basp/operation_strings.cpp
  src/convert_ip_endpoint.cpp
  src/datagram_socket.cpp
  src/defaults.cpp
  #src/endpoint_manager.cpp
  src/header.cpp
  src/host.cpp
  src/ip.cpp
  src/message_queue.cpp
  src/multiplexer.cpp
  #src/net/endpoint_manager_queue.cpp
  src/net/middleman.cpp
  src/net/middleman_backend.cpp
  src/net/packet_writer.cpp
  src/network_socket.cpp
  src/pipe_socket.cpp
  src/pollset_updater.cpp
  src/socket.cpp
  src/socket_manager.cpp
  src/stream_socket.cpp
  src/tcp_accept_socket.cpp
  src/tcp_stream_socket.cpp
  src/udp_datagram_socket.cpp
  src/worker.cpp
)

add_library(libcaf_net "${PROJECT_SOURCE_DIR}/cmake/dummy.cpp"
            $<TARGET_OBJECTS:libcaf_net_obj>)

generate_export_header(libcaf_net
                       EXPORT_MACRO_NAME CAF_NET_EXPORT
                       EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/caf/detail/net_export.hpp")

set_property(TARGET libcaf_net_obj PROPERTY POSITION_INDEPENDENT_CODE ON)

caf_net_set_default_properties(libcaf_net_obj libcaf_net)

target_include_directories(libcaf_net INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

add_library(CAF::net ALIAS libcaf_net)

set_target_properties(libcaf_net PROPERTIES
                      EXPORT_NAME net
                      SOVERSION ${CAF_VERSION}
                      VERSION ${CAF_LIB_VERSION}
                      OUTPUT_NAME caf_net)

# -- install library and header files ------------------------------------------

install(FILES "${CMAKE_BINARY_DIR}/caf/detail/net_export.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/caf/detail")

install(TARGETS libcaf_net
        EXPORT CAFTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT net
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT net
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT net)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT net
        FILES_MATCHING PATTERN "*.hpp")

# -- build unit tests ----------------------------------------------------------

if(NOT CAF_INC_ENABLE_TESTING)
  return()
endif()

add_executable(caf-net-test
               test/net-test.cpp
               $<TARGET_OBJECTS:libcaf_net_obj>)

caf_net_set_default_properties(caf-net-test)

target_include_directories(caf-net-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/test")

target_link_libraries(caf-net-test PRIVATE CAF::test)

caf_incubator_add_test_suites(caf-net-test
  accept_socket
  #application
  convert_ip_endpoint
  datagram_socket
  #datagram_transport
  #doorman
  #endpoint_manager
  header
  ip
  multiplexer
  #net.backend.tcp
  #net.basp.message_queue
  #net.basp.ping_pong
  #net.basp.worker
  net.length_prefix_framing
  net.web_socket
  network_socket
  pipe_socket
  socket
  socket_guard
  #stream_application
  stream_socket
  stream_transport
  #string_application
  tcp_sockets
  #transport_worker
  #transport_worker_dispatcher
  udp_datagram_socket
)
