cmake_minimum_required(VERSION 2.8.12)
project(caf_core C CXX)

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB_RECURSE LIBCAF_CORE_HDRS "caf/*.hpp")

# list cpp files excluding platform-dependent files
set(LIBCAF_CORE_SRCS
  src/abstract_actor.cpp
  src/abstract_channel.cpp
  src/abstract_composable_behavior.cpp
  src/abstract_coordinator.cpp
  src/abstract_group.cpp
  src/actor.cpp
  src/actor_addr.cpp
  src/actor_clock.cpp
  src/actor_companion.cpp
  src/actor_config.cpp
  src/actor_control_block.cpp
  src/actor_ostream.cpp
  src/actor_pool.cpp
  src/actor_proxy.cpp
  src/actor_registry.cpp
  src/actor_system.cpp
  src/actor_system_config.cpp
  src/append_hex.cpp
  src/atom.cpp
  src/attachable.cpp
  src/behavior.cpp
  src/behavior_impl.cpp
  src/behavior_stack.cpp
  src/blocking_actor.cpp
  src/blocking_behavior.cpp
  src/chars.cpp
  src/concatenated_tuple.cpp
  src/config_option.cpp
  src/config_option_adder.cpp
  src/config_option_set.cpp
  src/config_value.cpp
  src/decorated_tuple.cpp
  src/default_attachable.cpp
  src/defaults.cpp
  src/deserializer.cpp
  src/downstream_manager.cpp
  src/downstream_manager_base.cpp
  src/downstream_messages.cpp
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
  src/group.cpp
  src/group_manager.cpp
  src/group_module.cpp
  src/inbound_path.cpp
  src/ini_consumer.cpp
  src/invoke_result_visitor.cpp
  src/ipv4_address.cpp
  src/ipv4_subnet.cpp
  src/ipv6_address.cpp
  src/ipv6_subnet.cpp
  src/local_actor.cpp
  src/logger.cpp
  src/mailbox_element.cpp
  src/make_config_option.cpp
  src/match_case.cpp
  src/memory_managed.cpp
  src/merged_tuple.cpp
  src/message.cpp
  src/message_builder.cpp
  src/message_data.cpp
  src/message_handler.cpp
  src/message_view.cpp
  src/monitorable_actor.cpp
  src/node_id.cpp
  src/outbound_path.cpp
  src/pec.cpp
  src/pretty_type_name.cpp
  src/private_thread.cpp
  src/proxy_registry.cpp
  src/raise_error.cpp
  src/raw_event_based_actor.cpp
  src/ref_counted.cpp
  src/replies_to.cpp
  src/response_promise.cpp
  src/resumable.cpp
  src/ripemd_160.cpp
  src/rtti_pair.cpp
  src/runtime_settings_map.cpp
  src/scheduled_actor.cpp
  src/scoped_actor.cpp
  src/scoped_execution_unit.cpp
  src/sec.cpp
  src/sequencer.cpp
  src/serializer.cpp
  src/set_thread_name.cpp
  src/settings.cpp
  src/shared_spinlock.cpp
  src/simple_actor_clock.cpp
  src/skip.cpp
  src/splitter.cpp
  src/stream_aborter.cpp
  src/stream_manager.cpp
  src/stream_priority.cpp
  src/string_algorithms.cpp
  src/string_view.cpp
  src/stringification_inspector.cpp
  src/sync_request_bouncer.cpp
  src/term.cpp
  src/test_actor_clock.cpp
  src/test_coordinator.cpp
  src/thread_hook.cpp
  src/thread_safe_actor_clock.cpp
  src/tick_emitter.cpp
  src/timestamp.cpp
  src/try_match.cpp
  src/type_erased_tuple.cpp
  src/type_erased_value.cpp
  src/uniform_type_info_map.cpp
  src/unprofiled.cpp
  src/uri.cpp
  src/uri_builder.cpp
  src/uri_impl.cpp
  src/work_sharing.cpp
  src/work_stealing.cpp
)

# configure build_config.hpp header
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/build_config.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/caf/detail/build_config.hpp"
               @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/caf/detail/build_config.hpp"
        DESTINATION include/caf/detail
)
list(APPEND LIBCAF_CORE_HDRS
  "${CMAKE_CURRENT_BINARY_DIR}/caf/detail/build_config.hpp"
)

add_custom_target(libcaf_core)

# build shared library if not compiling static only
if (NOT CAF_BUILD_STATIC_ONLY)
  add_library(libcaf_core_shared SHARED ${LIBCAF_CORE_SRCS} ${LIBCAF_CORE_HDRS})
  target_link_libraries(libcaf_core_shared ${CAF_EXTRA_LDFLAGS})
  target_include_directories(libcaf_core_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  set_target_properties(libcaf_core_shared
    PROPERTIES
    SOVERSION ${CAF_VERSION}
    VERSION ${CAF_VERSION}
    OUTPUT_NAME caf_core
  )
  install(TARGETS libcaf_core_shared
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
  )
  add_dependencies(libcaf_core_shared libcaf_core)
endif ()

# build static library only if --build-static or --build-static-only was set
if (CAF_BUILD_STATIC_ONLY OR CAF_BUILD_STATIC)
  add_library(libcaf_core_static STATIC ${LIBCAF_CORE_HDRS} ${LIBCAF_CORE_SRCS})
  target_link_libraries(libcaf_core_static ${CAF_EXTRA_LDFLAGS})
  target_include_directories(libcaf_core_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  set_target_properties(libcaf_core_static PROPERTIES OUTPUT_NAME caf_core_static)
  install(TARGETS libcaf_core_static ARCHIVE DESTINATION lib)
  add_dependencies(libcaf_core_static libcaf_core)
endif ()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp"
)
