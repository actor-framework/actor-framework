cmake_minimum_required(VERSION 2.8)
project(caf_io C CXX)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB LIBCAF_IO_HDRS "caf/io/*.hpp" "caf/io/network/*.hpp")

# list cpp files excluding platform-dependent files
set (LIBCAF_IO_SRCS
     src/basp_broker.cpp
     src/broker.cpp
     src/max_msg_size.cpp
     src/middleman.cpp
     src/hook.cpp
     src/default_multiplexer.cpp
     src/publish.cpp
     src/publish_local_groups.cpp
     src/remote_actor.cpp
     src/remote_actor_proxy.cpp
     src/remote_group.cpp
     src/manager.cpp
     src/stream_manager.cpp
     src/unpublish.cpp
     src/acceptor_manager.cpp
     src/multiplexer.cpp)

# build shared library if not compiling static only
if (NOT "${CAF_BUILD_STATIC_ONLY}" STREQUAL "yes")
  add_library(libcaf_io SHARED ${LIBCAF_IO_SRCS} ${LIBCAF_IO_HDRS})
  target_link_libraries(libcaf_io ${LD_FLAGS} ${LIBCAF_CORE_LIBRARY})
  set(LIBRARY_SOVERSION ${CAF_VERSION_MAJOR})
  set_target_properties(libcaf_io
                        PROPERTIES
                        SOVERSION ${LIBRARY_SOVERSION}
                        VERSION ${CAF_VERSION}
                        OUTPUT_NAME caf_io)
  if(NOT WIN32)
    install(TARGETS libcaf_io LIBRARY DESTINATION lib)
  endif()
endif ()

# build static library only if --build-static or --build-static-only was set
if ("${CAF_BUILD_STATIC_ONLY}" STREQUAL "yes" OR "${CAF_BUILD_STATIC}" STREQUAL "yes")
  add_library(libcaf_ioStatic STATIC ${LIBCAF_IO_HDRS} ${LIBCAF_IO_SRCS})
  set_target_properties(libcaf_ioStatic PROPERTIES OUTPUT_NAME caf_io_static)
  if(NOT WIN32)
    install(TARGETS libcaf_ioStatic ARCHIVE DESTINATION lib)
  endif()
endif ()

link_directories(${LD_DIRS})
include_directories(. ${INCLUDE_DIRS})

# install includes
if(NOT WIN32)
  install(DIRECTORY caf/ DESTINATION include/caf FILES_MATCHING PATTERN "*.hpp")
  install(DIRECTORY cppa/ DESTINATION include/cppa FILES_MATCHING PATTERN "*.hpp")
endif()
