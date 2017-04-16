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
     src/abstract_composable_behavior.cpp
     src/abstract_coordinator.cpp
     src/abstract_downstream.cpp
     src/abstract_group.cpp
     src/abstract_upstream.cpp
     src/actor.cpp
     src/actor_addr.cpp
     src/actor_companion.cpp
     src/actor_config.cpp
     src/actor_control_block.cpp
     src/actor_ostream.cpp
     src/actor_pool.cpp
     src/actor_proxy.cpp
     src/actor_registry.cpp
     src/actor_system.cpp
     src/actor_system_config.cpp
     src/anycast.cpp
     src/atom.cpp
     src/attachable.cpp
     src/behavior.cpp
     src/behavior_impl.cpp
     src/behavior_stack.cpp
     src/blocking_actor.cpp
     src/blocking_behavior.cpp
     src/broadcast.cpp
     src/concatenated_tuple.cpp
     src/config_option.cpp
     src/decorated_tuple.cpp
     src/default_attachable.cpp
     src/deserializer.cpp
     src/downstream_path.cpp
     src/downstream_policy.cpp
     src/duration.cpp
     src/dynamic_message_data.cpp
     src/error.cpp
     src/event_based_actor.cpp
     src/execution_unit.cpp
     src/exit_reason.cpp
     src/forwarding_actor_proxy.cpp
     src/get_mac_addresses.cpp
     src/get_process_id.cpp
     src/get_root_uuid.cpp
     src/greedy.cpp
     src/group.cpp
     src/group_manager.cpp
     src/group_module.cpp
     src/incoming_stream_multiplexer.cpp
     src/invoke_result_visitor.cpp
     src/local_actor.cpp
     src/logger.cpp
     src/mailbox_element.cpp
     src/match_case.cpp
     src/memory.cpp
     src/memory_managed.cpp
     src/merged_tuple.cpp
     src/message.cpp
     src/message_builder.cpp
     src/message_data.cpp
     src/message_handler.cpp
     src/message_view.cpp
     src/monitorable_actor.cpp
     src/node_id.cpp
     src/outgoing_stream_multiplexer.cpp
     src/parse_ini.cpp
     src/private_thread.cpp
     src/proxy_registry.cpp
     src/raw_event_based_actor.cpp
     src/ref_counted.cpp
     src/replies_to.cpp
     src/response_promise.cpp
     src/resumable.cpp
     src/ripemd_160.cpp
     src/scheduled_actor.cpp
     src/scoped_actor.cpp
     src/scoped_execution_unit.cpp
     src/sec.cpp
     src/sequencer.cpp
     src/serializer.cpp
     src/shared_spinlock.cpp
     src/skip.cpp
     src/splitter.cpp
     src/stream.cpp
     src/stream_handler.cpp
     src/stream_msg_visitor.cpp
     src/stream_multiplexer.cpp
     src/stream_priority.cpp
     src/stream_sink.cpp
     src/stream_source.cpp
     src/stream_stage.cpp
     src/stringification_inspector.cpp
     src/sync_request_bouncer.cpp
     src/term.cpp
     src/test_coordinator.cpp
     src/timestamp.cpp
     src/try_match.cpp
     src/type_erased_tuple.cpp
     src/type_erased_value.cpp
     src/uniform_type_info_map.cpp
     src/unprofiled.cpp
     src/upstream_path.cpp
     src/upstream_policy.cpp
     src/work_sharing.cpp
     src/work_stealing.cpp)

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
  if (CYGWIN)
    install(TARGETS libcaf_core_shared RUNTIME DESTINATION bin)
  elseif (NOT WIN32)
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
