# -- collect header files ------------------------------------------------------

file(GLOB_RECURSE CAF_CORE_HEADERS "caf/*.hpp")

# -- dependencies --------------------------------------------------------------

if(NOT TARGET Threads::Threads)
  find_package(Threads REQUIRED)
endif()

set(LIBCAF_CORE_OPTIONAL_DEPENDENCIES "")

# Check whether we need to link against libatomic.
if(NOT CMAKE_CROSSCOMPILING)
  set(snippet "#include <cstdint>
               #include <atomic>
               std::atomic<uint64_t> x;
               int main(int, char**) { return static_cast<int>(x.load()); }")
  check_cxx_source_compiles("${snippet}" has_64bit_atomic)
  if(NOT has_64bit_atomic)
    set(required_libs_backup "${CMAKE_REQUIRED_LIBRARIES}")
    list(APPEND CMAKE_REQUIRED_LIBRARIES "atomic")
    check_cxx_source_compiles("${snippet}" has_64bit_atomic_with_libatomic)
    set(CMAKE_REQUIRED_LIBRARIES "${required_libs_backup}")
    if(NOT has_64bit_atomic_with_libatomic)
      message(FATAL_ERROR "Unable to compile code with std::atomic<uint64_t>")
    endif()
    list(APPEND LIBCAF_CORE_OPTIONAL_DEPENDENCIES atomic)
  endif()
endif()

if(MSVC)
    list(APPEND LIBCAF_CORE_OPTIONAL_DEPENDENCIES iphlpapi)
endif()

# -- generated files -----------------------------------------------------------

configure_file("${PROJECT_SOURCE_DIR}/cmake/build_config.hpp.in"
               "caf/detail/build_config.hpp"
               @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/caf/detail/build_config.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/caf/detail/")

# -- add targets ---------------------------------------------------------------

caf_add_component(
  core
  DEPENDENCIES
    PUBLIC
      Threads::Threads
      ${LIBCAF_CORE_OPTIONAL_DEPENDENCIES}
    PRIVATE
      CAF::internal
  ENUM_TYPES
    async.read_result
    async.write_result
    exit_reason
    flow.op.state
    intrusive.inbox_result
    invoke_message_result
    message_priority
    pec
    sec
    term
    thread_owner
  HEADERS
    ${CAF_CORE_HEADERS}
  SOURCES
    caf/abstract_actor.cpp
    caf/abstract_mailbox.cpp
    caf/action.cpp
    caf/action.test.cpp
    caf/actor.cpp
    caf/actor_addr.cpp
    caf/actor_clock.cpp
    caf/actor_companion.cpp
    caf/actor_companion.test.cpp
    caf/actor_config.cpp
    caf/actor_control_block.cpp
    caf/actor_factory.test.cpp
    caf/actor_from_state.test.cpp
    caf/actor_ostream.cpp
    caf/actor_pool.cpp
    caf/actor_pool.test.cpp
    caf/actor_profiler.cpp
    caf/actor_proxy.cpp
    caf/actor_registry.cpp
    caf/actor_registry.test.cpp
    caf/actor_system.cpp
    caf/actor_system.test.cpp
    caf/actor_system_config.cpp
    caf/actor_system_config.test.cpp
    caf/anon_mail.test.cpp
    caf/async/batch.cpp
    caf/async/batch.test.cpp
    caf/async/blocking_consumer.test.cpp
    caf/async/blocking_producer.test.cpp
    caf/async/consumer.cpp
    caf/async/consumer_adapter.test.cpp
    caf/async/execution_context.cpp
    caf/async/file.test.cpp
    caf/async/producer.cpp
    caf/async/producer_adapter.test.cpp
    caf/async/promise.test.cpp
    caf/async/publisher.test.cpp
    caf/async/spsc_buffer.test.cpp
    caf/async_mail.test.cpp
    caf/attachable.cpp
    caf/behavior.cpp
    caf/behavior.test.cpp
    caf/binary_deserializer.cpp
    caf/binary_serializer.cpp
    caf/blocking_actor.cpp
    caf/blocking_actor.test.cpp
    caf/byte_span.cpp
    caf/chrono.cpp
    caf/chrono.test.cpp
    caf/chunk.cpp
    caf/chunked_string.cpp
    caf/chunked_string.test.cpp
    caf/config_option.cpp
    caf/config_option.test.cpp
    caf/config_option_adder.cpp
    caf/config_option_set.cpp
    caf/config_value.cpp
    caf/config_value_reader.cpp
    caf/config_value_writer.cpp
    caf/const_typed_message_view.test.cpp
    caf/cow_string.test.cpp
    caf/cow_tuple.test.cpp
    caf/deep_to_string.test.cpp
    caf/default_attachable.cpp
    caf/delegate.test.cpp
    caf/deserializer.cpp
    caf/detached_actors.test.cpp
    caf/detail/abstract_worker.cpp
    caf/detail/abstract_worker_hub.cpp
    caf/detail/actor_local_printer.cpp
    caf/detail/atomic_ref_counted.cpp
    caf/detail/base64.cpp
    caf/detail/base64.test.cpp
    caf/detail/behavior_impl.cpp
    caf/detail/behavior_stack.cpp
    caf/detail/blocking_behavior.cpp
    caf/detail/bounds_checker.test.cpp
    caf/detail/config_consumer.cpp
    caf/detail/config_consumer.test.cpp
    caf/detail/default_mailbox.cpp
    caf/detail/default_mailbox.test.cpp
    caf/detail/format.test.cpp
    caf/detail/get_process_id.cpp
    caf/detail/glob_match.cpp
    caf/detail/ieee_754.test.cpp
    caf/detail/invoke_result_visitor.cpp
    caf/detail/json.cpp
    caf/detail/latch.cpp
    caf/detail/latch.test.cpp
    caf/detail/log_level_map.cpp
    caf/detail/log_level_map.test.cpp
    caf/detail/mailbox_factory.cpp
    caf/detail/mbr_list.test.cpp
    caf/detail/message_builder_element.cpp
    caf/detail/message_data.cpp
    caf/detail/meta_object.cpp
    caf/detail/meta_object.test.cpp
    caf/detail/monotonic_buffer_resource.cpp
    caf/detail/monotonic_buffer_resource.test.cpp
    caf/detail/parse.cpp
    caf/detail/parse.test.cpp
    caf/detail/parser/chars.cpp
    caf/detail/parser/read_bool.test.cpp
    caf/detail/parser/read_config.test.cpp
    caf/detail/parser/read_floating_point.test.cpp
    caf/detail/parser/read_number.test.cpp
    caf/detail/parser/read_number_or_timespan.test.cpp
    caf/detail/parser/read_signed_integer.test.cpp
    caf/detail/parser/read_string.test.cpp
    caf/detail/parser/read_timespan.test.cpp
    caf/detail/parser/read_unsigned_integer.test.cpp
    caf/detail/plain_ref_counted.cpp
    caf/detail/pretty_type_name.cpp
    caf/detail/print.cpp
    caf/detail/private_thread.cpp
    caf/detail/private_thread_pool.cpp
    caf/detail/private_thread_pool.test.cpp
    caf/detail/rfc3629.cpp
    caf/detail/rfc3629.test.cpp
    caf/detail/ring_buffer.test.cpp
    caf/detail/set_thread_name.cpp
    caf/detail/stream_bridge.cpp
    caf/detail/stringification_inspector.cpp
    caf/detail/sync_request_bouncer.cpp
    caf/detail/sync_ring_buffer.test.cpp
    caf/detail/thread_safe_actor_clock.cpp
    caf/detail/type_id_list_builder.cpp
    caf/detail/type_id_list_builder.test.cpp
    caf/detail/unique_function.test.cpp
    caf/dictionary.test.cpp
    caf/disposable.cpp
    caf/error.cpp
    caf/error.test.cpp
    caf/event_based_actor.cpp
    caf/event_based_actor.test.cpp
    caf/event_based_mail.test.cpp
    caf/execution_unit.cpp
    caf/expected.test.cpp
    caf/flow/concat_map.test.cpp
    caf/flow/coordinated.cpp
    caf/flow/coordinator.cpp
    caf/flow/flat_map.test.cpp
    caf/flow/for_each.test.cpp
    caf/flow/generation.test.cpp
    caf/flow/mixed.test.cpp
    caf/flow/multicaster.test.cpp
    caf/flow/observable.test.cpp
    caf/flow/observable_builder.cpp
    caf/flow/observe_on.test.cpp
    caf/flow/op/buffer.test.cpp
    caf/flow/op/cell.test.cpp
    caf/flow/op/concat.test.cpp
    caf/flow/op/defer.test.cpp
    caf/flow/op/empty.test.cpp
    caf/flow/op/fail.test.cpp
    caf/flow/op/interval.cpp
    caf/flow/op/interval.test.cpp
    caf/flow/op/mcast.test.cpp
    caf/flow/op/merge.test.cpp
    caf/flow/op/never.test.cpp
    caf/flow/op/prefix_and_tail.test.cpp
    caf/flow/op/publish.test.cpp
    caf/flow/op/pullable.cpp
    caf/flow/op/sample.test.cpp
    caf/flow/op/ucast.test.cpp
    caf/flow/op/zip_with.test.cpp
    caf/flow/scoped_coordinator.cpp
    caf/flow/single.test.cpp
    caf/flow/step/ignore_elements.test.cpp
    caf/flow/step/skip_last.test.cpp
    caf/flow/step/take_last.test.cpp
    caf/flow/string.test.cpp
    caf/flow/subscription.cpp
    caf/forwarding_actor_proxy.cpp
    caf/function_view.test.cpp
    caf/handles.test.cpp
    caf/hash/fnv.test.cpp
    caf/hash/sha1.cpp
    caf/hash/sha1.test.cpp
    caf/init_global_meta_objects.cpp
    caf/intrusive/lifo_inbox.test.cpp
    caf/intrusive/linked_list.test.cpp
    caf/intrusive/stack.test.cpp
    caf/intrusive_ptr.test.cpp
    caf/ipv4_address.cpp
    caf/ipv4_address.test.cpp
    caf/ipv4_endpoint.cpp
    caf/ipv4_endpoint.test.cpp
    caf/ipv4_subnet.cpp
    caf/ipv4_subnet.test.cpp
    caf/ipv6_address.cpp
    caf/ipv6_address.test.cpp
    caf/ipv6_endpoint.cpp
    caf/ipv6_endpoint.test.cpp
    caf/ipv6_subnet.cpp
    caf/ipv6_subnet.test.cpp
    caf/json_array.cpp
    caf/json_builder.cpp
    caf/json_object.cpp
    caf/json_reader.cpp
    caf/json_value.cpp
    caf/json_writer.cpp
    caf/load_inspector.cpp
    caf/local_actor.cpp
    caf/log/event.cpp
    caf/log/event.test.cpp
    caf/logger.cpp
    caf/mailbox_element.cpp
    caf/mailbox_element.test.cpp
    caf/make_config_option.cpp
    caf/message.cpp
    caf/message_builder.cpp
    caf/message_builder.test.cpp
    caf/message_handler.cpp
    caf/message_id.test.cpp
    caf/message_lifetime.test.cpp
    caf/messaging.test.cpp
    caf/metaprogramming.test.cpp
    caf/mtl.test.cpp
    caf/node_id.cpp
    caf/node_id.test.cpp
    caf/or_else.test.cpp
    caf/policy/select_all.test.cpp
    caf/policy/select_any.test.cpp
    caf/proxy_registry.cpp
    caf/raise_error.cpp
    caf/ref_counted.cpp
    caf/response_handle.test.cpp
    caf/response_promise.cpp
    caf/result.test.cpp
    caf/resumable.cpp
    caf/run_delayed.test.cpp
    caf/run_scheduled.test.cpp
    caf/save_inspector.cpp
    caf/scheduled_actor.cpp
    caf/scheduled_actor.test.cpp
    caf/scheduler.cpp
    caf/scoped_actor.cpp
    caf/scoped_execution_unit.cpp
    caf/sec.test.cpp
    caf/serial_reply.test.cpp
    caf/serialization.test.cpp
    caf/serializer.cpp
    caf/settings.cpp
    caf/settings.test.cpp
    caf/simple_timeout.test.cpp
    caf/skip.cpp
    caf/span.test.cpp
    caf/stream.cpp
    caf/stream.test.cpp
    caf/string_algorithms.cpp
    caf/string_algorithms.test.cpp
    caf/system_messages.test.cpp
    caf/telemetry/collector/prometheus.cpp
    caf/telemetry/collector/prometheus.test.cpp
    caf/telemetry/counter.test.cpp
    caf/telemetry/gauge.test.cpp
    caf/telemetry/histogram.test.cpp
    caf/telemetry/importer/process.cpp
    caf/telemetry/label.cpp
    caf/telemetry/label.test.cpp
    caf/telemetry/label_view.cpp
    caf/telemetry/metric.cpp
    caf/telemetry/metric_family.cpp
    caf/telemetry/metric_registry.cpp
    caf/telemetry/metric_registry.test.cpp
    caf/telemetry/timer.test.cpp
    caf/term.cpp
    caf/thread_hook.cpp
    caf/timestamp.cpp
    caf/tracing_data.cpp
    caf/tracing_data_factory.cpp
    caf/type_id.cpp
    caf/type_id_list.cpp
    caf/type_id_list.test.cpp
    caf/typed_behavior.test.cpp
    caf/typed_message_view.test.cpp
    caf/unit.test.cpp
    caf/unordered_flat_map.test.cpp
    caf/uri.cpp
    caf/uri.test.cpp
    caf/uri_builder.cpp
    caf/uuid.cpp
    caf/uuid.test.cpp
    caf/detail/beacon.cpp
  LEGACY_TEST_SOURCES
    tests/legacy/core-test.cpp
  LEGACY_TEST_SUITES
    actor_lifetime
    actor_profiler
    actor_termination
    config_option_set
    config_value
    constructor_attach
    dsl
    dynamic_spawn
    json_array
    json_builder
    json_object
    json_reader
    json_value
    json_writer
    message
    mixin.requester
    mixin.sender
    request_timeout
    response_promise
    stateful_actor
    thread_hook
    tracing_data
    typed_actor_view
    typed_response_promise
    typed_spawn
    typed_stream)

# Additional log component for (potentially critical) system-wide events.
caf_add_log_component(system)

if(NOT CAF_USE_STD_FORMAT)
  target_sources(libcaf_core PRIVATE caf/detail/format.cpp)
endif()

if(TARGET caf-core-legacy-test AND CAF_ENABLE_EXCEPTIONS)
  caf_add_legacy_test_suites(caf-core-legacy-test custom_exception_handler)
endif()
