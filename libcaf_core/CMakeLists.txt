# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE CAF_CORE_HEADERS "caf/*.hpp")

# -- auto generate to_string for enum types ------------------------------------

enum_to_string("caf/exit_reason.hpp" "exit_reason_strings.cpp")
enum_to_string("caf/intrusive/inbox_result.hpp" "inbox_result_strings.cpp")
enum_to_string("caf/intrusive/task_result.hpp" "task_result_strings.cpp")
enum_to_string("caf/invoke_message_result.hpp" "invoke_msg_result_strings.cpp")
enum_to_string("caf/message_priority.hpp" "message_priority_strings.cpp")
enum_to_string("caf/pec.hpp" "pec_strings.cpp")
enum_to_string("caf/sec.hpp" "sec_strings.cpp")
enum_to_string("caf/stream_priority.hpp" "stream_priority_strings.cpp")

# -- list cpp files for libcaf_core --------------------------------------------

set(CAF_CORE_SOURCES
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

# -- list cpp files for caf-core-test ------------------------------------------

set(CAF_CORE_TEST_SOURCES
  test/actor_clock.cpp
  test/actor_factory.cpp
  test/actor_lifetime.cpp
  test/actor_pool.cpp
  test/actor_profiler.cpp
  test/actor_registry.cpp
  test/actor_system_config.cpp
  test/actor_termination.cpp
  test/aout.cpp
  test/atom.cpp
  test/behavior.cpp
  test/blocking_actor.cpp
  test/broadcast_downstream_manager.cpp
  test/byte.cpp
  test/composable_behavior.cpp
  test/composition.cpp
  test/config_option.cpp
  test/config_option_set.cpp
  test/config_value.cpp
  test/config_value_adaptor.cpp
  test/constructor_attach.cpp
  test/continuous_streaming.cpp
  test/cow_tuple.cpp
  test/custom_exception_handler.cpp
  test/decorator/sequencer.cpp
  test/decorator/splitter.cpp
  test/deep_to_string.cpp
  test/detached_actors.cpp
  test/detail/bounds_checker.cpp
  test/detail/ini_consumer.cpp
  test/detail/limited_vector.cpp
  test/detail/parse.cpp
  test/detail/parser/read_atom.cpp
  test/detail/parser/read_bool.cpp
  test/detail/parser/read_floating_point.cpp
  test/detail/parser/read_ini.cpp
  test/detail/parser/read_number.cpp
  test/detail/parser/read_number_or_timespan.cpp
  test/detail/parser/read_signed_integer.cpp
  test/detail/parser/read_string.cpp
  test/detail/parser/read_timespan.cpp
  test/detail/parser/read_unsigned_integer.cpp
  test/detail/ringbuffer.cpp
  test/detail/ripemd_160.cpp
  test/detail/serialized_size.cpp
  test/detail/tick_emitter.cpp
  test/detail/unique_function.cpp
  test/detail/unordered_flat_map.cpp
  test/dictionary.cpp
  test/dynamic_spawn.cpp
  test/expected.cpp
  test/extract.cpp
  test/function_view.cpp
  test/fused_downstream_manager.cpp
  test/handles.cpp
  test/inbound_path.cpp
  test/inspector.cpp
  test/intrusive/drr_cached_queue.cpp
  test/intrusive/drr_queue.cpp
  test/intrusive/fifo_inbox.cpp
  test/intrusive/lifo_inbox.cpp
  test/intrusive/task_queue.cpp
  test/intrusive/wdrr_dynamic_multiplexed_queue.cpp
  test/intrusive/wdrr_fixed_multiplexed_queue.cpp
  test/intrusive_ptr.cpp
  test/ipv4_address.cpp
  test/ipv4_endpoint.cpp
  test/ipv4_subnet.cpp
  test/ipv6_address.cpp
  test/ipv6_endpoint.cpp
  test/ipv6_subnet.cpp
  test/local_group.cpp
  test/local_migration.cpp
  test/logger.cpp
  test/mailbox_element.cpp
  test/make_config_value_field.cpp
  test/match.cpp
  test/message.cpp
  test/message_id.cpp
  test/message_lifetime.cpp
  test/metaprogramming.cpp
  test/mixin/requester.cpp
  test/mixin/sender.cpp
  test/mock_streaming_classes.cpp
  test/native_streaming_classes.cpp
  test/optional.cpp
  test/or_else.cpp
  test/pipeline_streaming.cpp
  test/policy/categorized.cpp
  test/request_timeout.cpp
  test/result.cpp
  test/rtti_pair.cpp
  test/runtime_settings_map.cpp
  test/selective_streaming.cpp
  test/serial_reply.cpp
  test/serialization.cpp
  test/serializer_impl.cpp
  test/settings.cpp
  test/simple_timeout.cpp
  test/span.cpp
  test/stateful_actor.cpp
  test/streambuf.cpp
  test/string_algorithms.cpp
  test/string_view.cpp
  test/sum_type.cpp
  test/sum_type_token.cpp
  test/thread_hook.cpp
  test/to_string.cpp
  test/type_erased_tuple.cpp
  test/typed_response_promise.cpp
  test/typed_spawn.cpp
  test/unit.cpp
  test/uri.cpp
  test/variant.cpp
)

# -- add library target --------------------------------------------------------

add_library(libcaf_core_obj OBJECT ${CAF_CORE_SOURCES} ${CAF_CORE_HEADERS})

add_library(libcaf_core $<TARGET_OBJECTS:libcaf_core_obj>)

add_library(caf::core ALIAS libcaf_core)

target_link_libraries(libcaf_core PUBLIC ${CAF_EXTRA_LDFLAGS})

generate_export_header(libcaf_core_obj
                       EXPORT_MACRO_NAME CAF_CORE_EXPORT
                       EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/caf/detail/core_export.hpp"
                       STATIC_DEFINE CAF_STATIC_BUILD)

set_target_properties(libcaf_core PROPERTIES
                      EXPORT_NAME core
                      SOVERSION ${CAF_VERSION}
                      VERSION ${CAF_LIB_VERSION}
                      OUTPUT_NAME caf_core)

# -- install library and header files ------------------------------------------

install(FILES "${CMAKE_BINARY_DIR}/caf/detail/build_config.hpp"
              "${CMAKE_BINARY_DIR}/caf/detail/core_export.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/caf/detail")

install(TARGETS libcaf_core
        EXPORT CAFTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT core
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT core
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT core)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/caf"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT core
        FILES_MATCHING PATTERN "*.hpp")

# -- add this library to the global CAF_LIBRARIES ------------------------------

list(APPEND CAF_LIBRARIES libcaf_core)

set(CAF_LIBRARIES ${CAF_LIBRARIES} PARENT_SCOPE)

# -- build unit tests ----------------------------------------------------------

if(NOT CAF_NO_UNIT_TESTS)
  add_executable(caf-core-test
                 "${PROJECT_SOURCE_DIR}/libcaf_test/src/caf-test.cpp"
                 "${PROJECT_SOURCE_DIR}/libcaf_test/caf/test/unit_test.hpp"
                 "${PROJECT_SOURCE_DIR}/libcaf_test/caf/test/unit_test_impl.hpp"
                 ${CAF_CORE_TEST_SOURCES}
                 $<TARGET_OBJECTS:libcaf_core_obj>)
  target_link_libraries(caf-core-test ${CAF_EXTRA_LDFLAGS})
  add_test_suites(caf-core-test
                  "${CMAKE_CURRENT_SOURCE_DIR}"
                  ${CAF_CORE_TEST_SOURCES})
endif()

# -- backwards compatibility, TODO: remove -------------------------------------

if(BUILD_SHARED_LIBS)
  add_library(libcaf_core_shared ALIAS libcaf_core)
else()
  add_library(libcaf_core_static ALIAS libcaf_core)
endif()
