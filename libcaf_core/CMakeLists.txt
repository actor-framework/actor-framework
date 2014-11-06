cmake_minimum_required(VERSION 2.8)
project(caf_core C CXX)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(.)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB LIBCAF_CORE_HDRS "caf/*.hpp"
                           "caf/detail/*.hpp"
                           "caf/policy/*.hpp"
                           "caf/mixin/*.hpp"
                           "caf/scheduler/*.hpp"
                           "cppa/*.hpp")

# list cpp files excluding platform-dependent files
set (LIBCAF_CORE_SRCS
     src/abstract_actor.cpp
     src/abstract_channel.cpp
     src/abstract_group.cpp
     src/abstract_coordinator.cpp
     src/actor.cpp
     src/actor_addr.cpp
     src/actor_companion.cpp
     src/actor_namespace.cpp
     src/actor_ostream.cpp
     src/actor_proxy.cpp
     src/actor_registry.cpp
     src/attachable.cpp
     src/behavior.cpp
     src/behavior_stack.cpp
     src/behavior_impl.cpp
     src/binary_deserializer.cpp
     src/binary_serializer.cpp
     src/blocking_actor.cpp
     src/channel.cpp
     src/continue_helper.cpp
     src/decorated_tuple.cpp
     src/default_attachable.cpp
     src/demangle.cpp
     src/deserializer.cpp
     src/duration.cpp
     src/event_based_actor.cpp
     src/exception.cpp
     src/execution_unit.cpp
     src/exit_reason.cpp
     src/get_mac_addresses.cpp
     src/get_root_uuid.cpp
     src/group.cpp
     src/group_manager.cpp
     src/local_actor.cpp
     src/logging.cpp
     src/mailbox_element.cpp
     src/match.cpp
     src/memory.cpp
     src/memory_managed.cpp
     src/message.cpp
     src/message_builder.cpp
     src/message_data.cpp
     src/message_handler.cpp
     src/node_id.cpp
     src/ref_counted.cpp
     src/response_promise.cpp
     src/resumable.cpp
     src/ripemd_160.cpp
     src/scoped_actor.cpp
     src/set_scheduler.cpp
     src/serializer.cpp
     src/shared_spinlock.cpp
     src/shutdown.cpp
     src/singletons.cpp
     src/string_algorithms.cpp
     src/string_serialization.cpp
     src/sync_request_bouncer.cpp
     src/to_uniform_name.cpp
     src/uniform_type_info.cpp
     src/uniform_type_info_map.cpp)

# build shared library if not compiling static only
if (NOT "${CAF_BUILD_STATIC_ONLY}" STREQUAL "yes")
  add_library(libcaf_core SHARED ${LIBCAF_CORE_SRCS} ${LIBCAF_CORE_HDRS})
  target_link_libraries(libcaf_core ${LD_FLAGS})
  set(LIBRARY_SOVERSION ${CAF_VERSION_MAJOR})
  set_target_properties(libcaf_core
                        PROPERTIES
                        SOVERSION ${LIBRARY_SOVERSION}
                        VERSION ${CAF_VERSION}
                        OUTPUT_NAME caf_core)
  if(NOT WIN32)
    install(TARGETS libcaf_core LIBRARY DESTINATION lib)
  endif()
endif ()

# build static library only if --build-static or --build-static-only was set
if ("${CAF_BUILD_STATIC_ONLY}" STREQUAL "yes" OR "${CAF_BUILD_STATIC}" STREQUAL "yes")
  add_library(libcaf_coreStatic STATIC ${LIBCAF_CORE_HDRS} ${LIBCAF_CORE_SRCS})
  set_target_properties(libcaf_coreStatic PROPERTIES OUTPUT_NAME caf_core_static)
  if(NOT WIN32)
    install(TARGETS libcaf_coreStatic ARCHIVE DESTINATION lib)
  endif()
endif ()

link_directories(${LD_DIRS})
