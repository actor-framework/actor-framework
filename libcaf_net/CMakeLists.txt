cmake_minimum_required(VERSION 2.8.12)
project(caf_net C CXX)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB_RECURSE LIBCAF_NET_HDRS "caf/*.hpp")

# list cpp files excluding platform-dependent files
set(LIBCAF_NET_SRCS
  src/actor_proxy_impl.cpp
  src/datagram_socket.cpp
  src/endpoint_manager.cpp
  src/host.cpp
  src/ip.cpp
  src/multiplexer.cpp
  src/network_socket.cpp
  src/pipe_socket.cpp
  src/pollset_updater.cpp
  src/socket.cpp
  src/socket_manager.cpp
  src/stream_socket.cpp
  src/tcp_stream_socket.cpp
  src/tcp_accept_socket.cpp
)

add_custom_target(libcaf_net)

# build shared library if not compiling static only
if (NOT CAF_BUILD_STATIC_ONLY)
  add_library(libcaf_net_shared SHARED ${LIBCAF_NET_SRCS} ${LIBCAF_NET_HDRS})
  target_link_libraries(libcaf_net_shared ${CAF_EXTRA_LDFLAGS} ${CAF_LIBRARY_CORE})
  target_include_directories(libcaf_net_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  set_target_properties(libcaf_net_shared
    PROPERTIES
    SOVERSION ${CAF_VERSION}
    VERSION ${CAF_VERSION}
    OUTPUT_NAME caf_net
  )
  install(TARGETS libcaf_net_shared
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
  )
  add_dependencies(libcaf_net_shared libcaf_net)
endif ()

# build static library only if --build-static or --build-static-only was set
if (CAF_BUILD_STATIC_ONLY OR CAF_BUILD_STATIC)
  add_library(libcaf_net_static STATIC ${LIBCAF_NET_HDRS} ${LIBCAF_NET_SRCS})
  target_link_libraries(libcaf_net_static ${CAF_EXTRA_LDFLAGS} ${CAF_LIBRARY_CORE_STATIC})
  target_include_directories(libcaf_net_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  set_target_properties(libcaf_net_static PROPERTIES OUTPUT_NAME caf_net_static)
  install(TARGETS libcaf_net_static ARCHIVE DESTINATION lib)
  add_dependencies(libcaf_net_static libcaf_net)
endif ()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp")
