cmake_minimum_required(VERSION 2.8)
project(caf_core C CXX)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

include_directories(.)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB_RECURSE LIBCAF_CORE_HDRS "caf/*.hpp")

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
     src/actor_pool.cpp
     src/actor_proxy.cpp
     src/actor_registry.cpp
     src/attachable.cpp
     src/announce_actor_type.cpp
     src/behavior.cpp
     src/behavior_stack.cpp
     src/behavior_impl.cpp
     src/binary_deserializer.cpp
     src/binary_serializer.cpp
     src/blocking_actor.cpp
     src/channel.cpp
     src/concatenated_tuple.cpp
     src/continue_helper.cpp
     src/decorated_tuple.cpp
     src/default_attachable.cpp
     src/deserializer.cpp
     src/detached_threads.cpp
     src/duration.cpp
     src/either.cpp
     src/event_based_actor.cpp
     src/exception.cpp
     src/execution_unit.cpp
     src/exit_reason.cpp
     src/forwarding_actor_proxy.cpp
     src/get_mac_addresses.cpp
     src/get_process_id.cpp
     src/get_root_uuid.cpp
     src/group.cpp
     src/group_manager.cpp
     src/match_case.cpp
     src/local_actor.cpp
     src/logging.cpp
     src/mailbox_element.cpp
     src/memory.cpp
     src/memory_managed.cpp
     src/message.cpp
     src/message_builder.cpp
     src/message_data.cpp
     src/message_handler.cpp
     src/node_id.cpp
     src/parse_config.cpp
     src/parse_ini.cpp
     src/ref_counted.cpp
     src/response_promise.cpp
     src/replies_to.cpp
     src/resumable.cpp
     src/ripemd_160.cpp
     src/run_sub_unit_test.cpp
     src/scoped_actor.cpp
     src/set_scheduler.cpp
     src/serializer.cpp
     src/shared_spinlock.cpp
     src/shutdown.cpp
     src/singletons.cpp
     src/string_serialization.cpp
     src/sync_request_bouncer.cpp
     src/try_match.cpp
     src/uniform_type_info.cpp
     src/uniform_type_info_map.cpp
     src/whereis.cpp)

add_custom_target(libcaf_core)

# build shared library if not compiling static only
if (NOT CAF_BUILD_STATIC_ONLY)
  add_library(libcaf_core_shared SHARED ${LIBCAF_CORE_SRCS} ${LIBCAF_CORE_HDRS})
  target_link_libraries(libcaf_core_shared ${LD_FLAGS})
  set_target_properties(libcaf_core_shared
                        PROPERTIES
                        SOVERSION ${CAF_VERSION}
                        VERSION ${CAF_VERSION}
                        OUTPUT_NAME caf_core)
  if(NOT WIN32)
    install(TARGETS libcaf_core_shared LIBRARY DESTINATION lib)
  endif()
  add_dependencies(libcaf_core_shared libcaf_core)
endif ()

# build static library only if --build-static or --build-static-only was set
if (CAF_BUILD_STATIC_ONLY OR CAF_BUILD_STATIC)
  add_library(libcaf_core_static STATIC ${LIBCAF_CORE_HDRS} ${LIBCAF_CORE_SRCS})
  target_link_libraries(libcaf_core_static ${LD_FLAGS})
  set_target_properties(libcaf_core_static PROPERTIES OUTPUT_NAME caf_core_static)
  if(NOT WIN32)
    install(TARGETS libcaf_core_static ARCHIVE DESTINATION lib)
  endif()
  add_dependencies(libcaf_core_static libcaf_core)
endif ()

link_directories(${LD_DIRS})
include_directories(. ${INCLUDE_DIRS})
