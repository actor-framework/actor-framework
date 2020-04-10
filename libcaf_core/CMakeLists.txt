# -- get header files for creating "proper" XCode projects ---------------------

file(GLOB_RECURSE CAF_CORE_HEADERS "caf/*.hpp")

# -- add consistency checks for enum to_string implementations -----------------

add_enum_consistency_check("caf/sec.hpp" "src/sec_strings.cpp")
add_enum_consistency_check("caf/pec.hpp" "src/pec_strings.cpp")
add_enum_consistency_check("caf/stream_priority.hpp"
                           "src/stream_priority_strings.cpp")
add_enum_consistency_check("caf/exit_reason.hpp"
                           "src/exit_reason_strings.cpp")
add_enum_consistency_check("caf/invoke_message_result.hpp"
                           "src/invoke_msg_result_strings.cpp")
add_enum_consistency_check("caf/message_priority.hpp"
                           "src/message_priority_strings.cpp")
add_enum_consistency_check("caf/intrusive/inbox_result.hpp"
                           "src/intrusive/inbox_result_strings.cpp")
add_enum_consistency_check("caf/intrusive/task_result.hpp"
                           "src/intrusive/task_result_strings.cpp")

# -- add library target --------------------------------------------------------

add_library(libcaf_core_obj OBJECT ${CAF_CORE_HEADERS}
  src/abstract_actor.cpp
  src/abstract_channel.cpp
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
  src/attachable.cpp
  src/behavior.cpp
  src/binary_deserializer.cpp
  src/binary_serializer.cpp
  src/blocking_actor.cpp
  src/complexity_based_credit_controller.cpp
  src/config_option.cpp
  src/config_option_adder.cpp
  src/config_option_set.cpp
  src/config_value.cpp
  src/credit_controller.cpp
  src/decorator/sequencer.cpp
  src/default_attachable.cpp
  src/defaults.cpp
  src/deserializer.cpp
  src/detail/abstract_worker.cpp
  src/detail/abstract_worker_hub.cpp
  src/detail/append_percent_encoded.cpp
  src/detail/behavior_impl.cpp
  src/detail/behavior_stack.cpp
  src/detail/blocking_behavior.cpp
  src/detail/get_mac_addresses.cpp
  src/detail/get_process_id.cpp
  src/detail/get_root_uuid.cpp
  src/detail/ini_consumer.cpp
  src/detail/invoke_result_visitor.cpp
  src/detail/message_builder_element.cpp
  src/detail/message_data.cpp
  src/detail/meta_object.cpp
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
  src/detail/type_id_list_builder.cpp
  src/detail/uri_impl.cpp
  src/downstream_manager.cpp
  src/downstream_manager_base.cpp
  src/error.cpp
  src/event_based_actor.cpp
  src/execution_unit.cpp
  src/exit_reason_strings.cpp
  src/forwarding_actor_proxy.cpp
  src/group.cpp
  src/group_manager.cpp
  src/group_module.cpp
  src/inbound_path.cpp
  src/init_global_meta_objects.cpp
  src/intrusive/inbox_result_strings.cpp
  src/intrusive/task_result_strings.cpp
  src/invoke_msg_result_strings.cpp
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
  src/memory_managed.cpp
  src/message.cpp
  src/message_builder.cpp
  src/message_handler.cpp
  src/message_priority_strings.cpp
  src/monitorable_actor.cpp
  src/node_id.cpp
  src/outbound_path.cpp
  src/pec_strings.cpp
  src/policy/downstream_messages.cpp
  src/policy/unprofiled.cpp
  src/policy/work_sharing.cpp
  src/policy/work_stealing.cpp
  src/proxy_registry.cpp
  src/raise_error.cpp
  src/ref_counted.cpp
  src/replies_to.cpp
  src/response_promise.cpp
  src/resumable.cpp
  src/scheduled_actor.cpp
  src/scheduler/abstract_coordinator.cpp
  src/scheduler/test_coordinator.cpp
  src/scoped_actor.cpp
  src/scoped_execution_unit.cpp
  src/sec_strings.cpp
  src/serializer.cpp
  src/settings.cpp
  src/size_based_credit_controller.cpp
  src/skip.cpp
  src/stream_aborter.cpp
  src/stream_manager.cpp
  src/stream_priority_strings.cpp
  src/string_algorithms.cpp
  src/string_view.cpp
  src/term.cpp
  src/test_credit_controller.cpp
  src/thread_hook.cpp
  src/timestamp.cpp
  src/tracing_data.cpp
  src/tracing_data_factory.cpp
  src/type_id_list.cpp
  src/uri.cpp
  src/uri_builder.cpp
  src/uuid.cpp
)

target_include_directories(libcaf_core_obj PRIVATE
                           "${CMAKE_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(libcaf_core "${PROJECT_SOURCE_DIR}/cmake/dummy.cpp"
            $<TARGET_OBJECTS:libcaf_core_obj>)

target_include_directories(libcaf_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_include_directories(libcaf_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)

add_library(CAF::core ALIAS libcaf_core)

# -- dependencies and compiler flags -------------------------------------------

if(BUILD_SHARED_LIBS AND NOT WIN32)
  target_compile_options(libcaf_core PRIVATE -fPIC)
  target_compile_options(libcaf_core_obj PRIVATE -fPIC)
endif()

if(NOT TARGET Threads::Threads)
  find_package(Threads REQUIRED)
endif()

target_link_libraries(libcaf_core PUBLIC Threads::Threads)

if(CAF_SANITIZERS)
  target_link_libraries(caf-incubator PUBLIC -fsanitize=${CAF_SANITIZERS})
endif()

# -- API exports ---------------------------------------------------------------

generate_export_header(libcaf_core
                       EXPORT_MACRO_NAME CAF_CORE_EXPORT
                       EXPORT_FILE_NAME "${CMAKE_BINARY_DIR}/caf/detail/core_export.hpp"
                       STATIC_DEFINE CAF_STATIC_BUILD)

target_compile_definitions(libcaf_core_obj PRIVATE libcaf_core_EXPORTS)

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

# -- build unit tests ----------------------------------------------------------

if(NOT CAF_BUILD_TESTING)
  return()
endif()

add_executable(caf-core-test
  test/core-test.cpp
  $<TARGET_OBJECTS:libcaf_core_obj>)

target_include_directories(caf-core-test PRIVATE
                           "${CMAKE_CURRENT_SOURCE_DIR}/test")

target_compile_definitions(caf-core-test PRIVATE libcaf_core_EXPORTS)

target_link_libraries(caf-core-test PUBLIC CAF::test)

target_include_directories(caf-core-test PRIVATE
                           "${CMAKE_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")

caf_add_test_suites(caf-core-test
  actor_clock
  actor_factory
  actor_lifetime
  actor_pool
  actor_profiler
  actor_registry
  actor_system_config
  actor_termination
  aout
  behavior
  binary_deserializer
  binary_serializer
  blocking_actor
  broadcast_downstream_manager
  byte
  composition
  config_option
  config_option_set
  config_value
  config_value_adaptor
  const_typed_message_view
  constructor_attach
  continuous_streaming
  cow_tuple
  custom_exception_handler
  decorator.sequencer
  deep_to_string
  detached_actors
  detail.bounds_checker
  detail.ini_consumer
  detail.limited_vector
  detail.meta_object
  detail.parse
  detail.parser.read_bool
  detail.parser.read_floating_point
  detail.parser.read_ini
  detail.parser.read_number
  detail.parser.read_number_or_timespan
  detail.parser.read_signed_integer
  detail.parser.read_string
  detail.parser.read_timespan
  detail.parser.read_unsigned_integer
  detail.ringbuffer
  detail.ripemd_160
  detail.serialized_size
  detail.tick_emitter
  detail.type_id_list_builder
  detail.unique_function
  detail.unordered_flat_map
  dictionary
  dynamic_spawn
  error
  expected
  function_view
  fused_downstream_manager
  handles
  hash.fnv
  inspector
  intrusive.drr_cached_queue
  intrusive.drr_queue
  intrusive.fifo_inbox
  intrusive.lifo_inbox
  intrusive.task_queue
  intrusive.wdrr_dynamic_multiplexed_queue
  intrusive.wdrr_fixed_multiplexed_queue
  intrusive_ptr
  ipv4_address
  ipv4_endpoint
  ipv4_subnet
  ipv6_address
  ipv6_endpoint
  ipv6_subnet
  local_group
  logger
  mailbox_element
  make_config_value_field
  message
  message_id
  message_lifetime
  metaprogramming
  mixin.requester
  mixin.sender
  mock_streaming_classes
  native_streaming_classes
  node_id
  optional
  or_else
  pipeline_streaming
  policy.categorized
  policy.select_all
  policy.select_any
  request_timeout
  result
  selective_streaming
  serialization
  settings
  simple_timeout
  span
  stateful_actor
  string_algorithms
  string_view
  sum_type
  thread_hook
  tracing_data
  type_id_list
  typed_behavior
  typed_message_view
  typed_response_promise
  typed_spawn
  unit
  uri
  uuid
  variant
)
