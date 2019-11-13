# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE LIBCAF_CORE_HDRS "caf/*.hpp")

# -- auto generate to_string for enum types ------------------------------------

enum_to_string("caf/exit_reason.hpp" "exit_reason_strings.cpp")
enum_to_string("caf/intrusive/inbox_result.hpp" "inbox_result_strings.cpp")
enum_to_string("caf/intrusive/task_result.hpp" "task_result_strings.cpp")
enum_to_string("caf/invoke_message_result.hpp" "invoke_msg_result_strings.cpp")
enum_to_string("caf/message_priority.hpp" "message_priority_strings.cpp")
enum_to_string("caf/pec.hpp" "pec_strings.cpp")
enum_to_string("caf/sec.hpp" "sec_strings.cpp")
enum_to_string("caf/stream_priority.hpp" "stream_priority_strings.cpp")

# -- list cpp files ------------------------------------------------------------

set(LIBCAF_CORE_SRCS
  "${CMAKE_CURRENT_BINARY_DIR}/exit_reason_strings.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/inbox_result_strings.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/invoke_msg_result_strings.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/message_priority_strings.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/pec_strings.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/sec_strings.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/stream_priority_strings.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/task_result_strings.cpp"
  src/abstract_actor.cpp
  src/abstract_channel.cpp
  src/abstract_composable_behavior.cpp
  src/abstract_group.cpp
  src/actor.cpp
  src/actor_addr.cpp
  src/actor_clock.cpp
  src/actor_companion.cpp
  src/actor_config.cpp
  src/actor_control_block.cpp
  src/actor_ostream.cpp
  src/actor_pool.cpp
  src/actor_profiler.cpp
  src/actor_proxy.cpp
  src/actor_registry.cpp
  src/actor_system.cpp
  src/actor_system_config.cpp
  src/atom.cpp
  src/attachable.cpp
  src/behavior.cpp
  src/binary_deserializer.cpp
  src/blocking_actor.cpp
  src/config_option.cpp
  src/config_option_adder.cpp
  src/config_option_set.cpp
  src/config_value.cpp
  src/decorator/sequencer.cpp
  src/decorator/splitter.cpp
  src/default_attachable.cpp
  src/defaults.cpp
  src/deserializer.cpp
  src/detail/abstract_worker.cpp
  src/detail/abstract_worker_hub.cpp
  src/detail/append_hex.cpp
  src/detail/append_percent_encoded.cpp
  src/detail/behavior_impl.cpp
  src/detail/behavior_stack.cpp
  src/detail/blocking_behavior.cpp
  src/detail/concatenated_tuple.cpp
  src/detail/decorated_tuple.cpp
  src/detail/dynamic_message_data.cpp
  src/detail/fnv_hash.cpp
  src/detail/get_mac_addresses.cpp
  src/detail/get_process_id.cpp
  src/detail/get_root_uuid.cpp
  src/detail/ini_consumer.cpp
  src/detail/invoke_result_visitor.cpp
  src/detail/merged_tuple.cpp
  src/detail/message_data.cpp
  src/detail/parse.cpp
  src/detail/parser/chars.cpp
  src/detail/pretty_type_name.cpp
  src/detail/private_thread.cpp
  src/detail/ripemd_160.cpp
  src/detail/serialized_size.cpp
  src/detail/set_thread_name.cpp
  src/detail/shared_spinlock.cpp
  src/detail/simple_actor_clock.cpp
  src/detail/stringification_inspector.cpp
  src/detail/sync_request_bouncer.cpp
  src/detail/test_actor_clock.cpp
  src/detail/thread_safe_actor_clock.cpp
  src/detail/tick_emitter.cpp
  src/detail/try_match.cpp
  src/detail/uri_impl.cpp
  src/downstream_manager.cpp
  src/downstream_manager_base.cpp
  src/duration.cpp
  src/error.cpp
  src/event_based_actor.cpp
  src/execution_unit.cpp
  src/exit_reason.cpp
  src/forwarding_actor_proxy.cpp
  src/group.cpp
  src/group_manager.cpp
  src/group_module.cpp
  src/inbound_path.cpp
  src/ipv4_address.cpp
  src/ipv4_endpoint.cpp
  src/ipv4_subnet.cpp
  src/ipv6_address.cpp
  src/ipv6_endpoint.cpp
  src/ipv6_subnet.cpp
  src/local_actor.cpp
  src/logger.cpp
  src/mailbox_element.cpp
  src/make_config_option.cpp
  src/match_case.cpp
  src/memory_managed.cpp
  src/message.cpp
  src/message_builder.cpp
  src/message_handler.cpp
  src/message_view.cpp
  src/monitorable_actor.cpp
  src/node_id.cpp
  src/outbound_path.cpp
  src/pec.cpp
  src/policy/downstream_messages.cpp
  src/policy/unprofiled.cpp
  src/policy/work_sharing.cpp
  src/policy/work_stealing.cpp
  src/proxy_registry.cpp
  src/raise_error.cpp
  src/raw_event_based_actor.cpp
  src/ref_counted.cpp
  src/replies_to.cpp
  src/response_promise.cpp
  src/resumable.cpp
  src/rtti_pair.cpp
  src/runtime_settings_map.cpp
  src/scheduled_actor.cpp
  src/scheduler/abstract_coordinator.cpp
  src/scheduler/test_coordinator.cpp
  src/scoped_actor.cpp
  src/scoped_execution_unit.cpp
  src/sec.cpp
  src/serializer.cpp
  src/settings.cpp
  src/skip.cpp
  src/stream_aborter.cpp
  src/stream_manager.cpp
  src/string_algorithms.cpp
  src/string_view.cpp
  src/term.cpp
  src/thread_hook.cpp
  src/timestamp.cpp
  src/type_erased_tuple.cpp
  src/type_erased_value.cpp
  src/uniform_type_info_map.cpp
  src/uri.cpp
  src/uri_builder.cpp
)

# -- generate build config header ----------------------------------------------

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/build_config.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/caf/detail/build_config.hpp"
               @ONLY)

# -- add library target --------------------------------------------------------

add_library(libcaf_core ${LIBCAF_CORE_SRCS} ${LIBCAF_CORE_HDRS})

add_library(caf::core ALIAS libcaf_core)

# TODO: only exists for backwards compatibility, remove with CAF 0.19
if(BUILD_SHARED_LIBS)
  add_library(libcaf_core_shared ALIAS libcaf_core)
else()
  add_library(libcaf_core_static ALIAS libcaf_core)
endif()

target_link_libraries(libcaf_core PUBLIC ${CAF_EXTRA_LDFLAGS})

generate_export_header(libcaf_core
                       EXPORT_MACRO_NAME CAF_CORE_EXPORT
                       EXPORT_FILE_NAME "caf/detail/core_export.hpp"
                       STATIC_DEFINE CAF_STATIC_BUILD)

target_include_directories(libcaf_core PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                           $<INSTALL_INTERFACE:include>)

set_target_properties(libcaf_core PROPERTIES
                      EXPORT_NAME core
                      SOVERSION ${CAF_VERSION}
                      VERSION ${CAF_LIB_VERSION}
                      OUTPUT_NAME caf_core)

# -- install library and header files ------------------------------------------

install(TARGETS libcaf_core
        EXPORT CAFTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT core
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT core
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT core)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT core
        FILES_MATCHING PATTERN "*.hpp")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/caf/detail/build_config.hpp"
              "${CMAKE_CURRENT_BINARY_DIR}/caf/detail/core_export.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/caf/detail")

# -- add this library to the global CAF_LIBRARIES ------------------------------

list(APPEND CAF_LIBRARIES libcaf_core)

set(CAF_LIBRARIES ${CAF_LIBRARIES} PARENT_SCOPE)
