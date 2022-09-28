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
    flow.observable_state
    flow.observer_state
    intrusive.inbox_result
    intrusive.task_result
    invoke_message_result
    message_priority
    pec
    sec
  HEADERS
    ${CAF_CORE_HEADERS}
  SOURCES
    src/abstract_actor.cpp
    src/abstract_channel.cpp
    src/abstract_group.cpp
    src/action.cpp
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
    src/async/batch.cpp
    src/async/consumer.cpp
    src/async/execution_context.cpp
    src/async/producer.cpp
    src/attachable.cpp
    src/behavior.cpp
    src/binary_deserializer.cpp
    src/binary_serializer.cpp
    src/blocking_actor.cpp
    src/config_option.cpp
    src/config_option_adder.cpp
    src/config_option_set.cpp
    src/config_value.cpp
    src/config_value_reader.cpp
    src/config_value_writer.cpp
    src/decorator/sequencer.cpp
    src/default_attachable.cpp
    src/deserializer.cpp
    src/detail/abstract_worker.cpp
    src/detail/abstract_worker_hub.cpp
    src/detail/atomic_ref_counted.cpp
    src/detail/base64.cpp
    src/detail/behavior_impl.cpp
    src/detail/behavior_stack.cpp
    src/detail/blocking_behavior.cpp
    src/detail/config_consumer.cpp
    src/detail/get_mac_addresses.cpp
    src/detail/get_process_id.cpp
    src/detail/get_root_uuid.cpp
    src/detail/glob_match.cpp
    src/detail/group_tunnel.cpp
    src/detail/invoke_result_visitor.cpp
    src/detail/json.cpp
    src/detail/latch.cpp
    src/detail/local_group_module.cpp
    src/detail/message_builder_element.cpp
    src/detail/message_data.cpp
    src/detail/meta_object.cpp
    src/detail/monotonic_buffer_resource.cpp
    src/detail/parse.cpp
    src/detail/parser/chars.cpp
    src/detail/plain_ref_counted.cpp
    src/detail/pretty_type_name.cpp
    src/detail/print.cpp
    src/detail/private_thread.cpp
    src/detail/private_thread_pool.cpp
    src/detail/ripemd_160.cpp
    src/detail/set_thread_name.cpp
    src/detail/shared_spinlock.cpp
    src/detail/stringification_inspector.cpp
    src/detail/sync_request_bouncer.cpp
    src/detail/test_actor_clock.cpp
    src/detail/thread_safe_actor_clock.cpp
    src/detail/type_id_list_builder.cpp
    src/disposable.cpp
    src/error.cpp
    src/event_based_actor.cpp
    src/execution_unit.cpp
    src/flow/coordinated.cpp
    src/flow/coordinator.cpp
    src/flow/observable_builder.cpp
    src/flow/op/interval.cpp
    src/flow/scoped_coordinator.cpp
    src/flow/subscription.cpp
    src/forwarding_actor_proxy.cpp
    src/group.cpp
    src/group_manager.cpp
    src/group_module.cpp
    src/hash/sha1.cpp
    src/init_global_meta_objects.cpp
    src/ipv4_address.cpp
    src/ipv4_endpoint.cpp
    src/ipv4_subnet.cpp
    src/ipv6_address.cpp
    src/ipv6_endpoint.cpp
    src/ipv6_subnet.cpp
    src/json_reader.cpp
    src/json_writer.cpp
    src/load_inspector.cpp
    src/local_actor.cpp
    src/logger.cpp
    src/mailbox_element.cpp
    src/make_config_option.cpp
    src/message.cpp
    src/message_builder.cpp
    src/message_handler.cpp
    src/monitorable_actor.cpp
    src/node_id.cpp
    src/policy/unprofiled.cpp
    src/policy/work_sharing.cpp
    src/policy/work_stealing.cpp
    src/proxy_registry.cpp
    src/raise_error.cpp
    src/ref_counted.cpp
    src/replies_to.cpp
    src/response_promise.cpp
    src/resumable.cpp
    src/save_inspector.cpp
    src/scheduled_actor.cpp
    src/scheduler/abstract_coordinator.cpp
    src/scheduler/test_coordinator.cpp
    src/scoped_actor.cpp
    src/scoped_execution_unit.cpp
    src/serializer.cpp
    src/settings.cpp
    src/skip.cpp
    src/string_algorithms.cpp
    src/string_view.cpp
    src/telemetry/collector/prometheus.cpp
    src/telemetry/importer/process.cpp
    src/telemetry/label.cpp
    src/telemetry/label_view.cpp
    src/telemetry/metric.cpp
    src/telemetry/metric_family.cpp
    src/telemetry/metric_registry.cpp
    src/term.cpp
    src/thread_hook.cpp
    src/timestamp.cpp
    src/tracing_data.cpp
    src/tracing_data_factory.cpp
    src/type_id.cpp
    src/type_id_list.cpp
    src/uri.cpp
    src/uri_builder.cpp
    src/uuid.cpp
  TEST_SOURCES
    test/core-test.cpp
    test/nasty.cpp
  TEST_SUITES
    action
    actor_clock
    actor_factory
    actor_lifetime
    actor_pool
    actor_profiler
    actor_registry
    actor_system_config
    actor_termination
    aout
    async.blocking_consumer
    async.blocking_producer
    async.consumer_adapter
    async.producer_adapter
    async.promise
    async.spsc_buffer
    behavior
    binary_deserializer
    binary_serializer
    blocking_actor
    composition
    config_option
    config_option_set
    config_value
    config_value_reader
    config_value_writer
    const_typed_message_view
    constructor_attach
    cow_string
    cow_tuple
    decorator.sequencer
    deep_to_string
    detached_actors
    detail.base64
    detail.bounds_checker
    detail.config_consumer
    detail.group_tunnel
    detail.ieee_754
    detail.json
    detail.latch
    detail.local_group_module
    detail.meta_object
    detail.monotonic_buffer_resource
    detail.parse
    detail.parser.read_bool
    detail.parser.read_config
    detail.parser.read_floating_point
    detail.parser.read_number
    detail.parser.read_number_or_timespan
    detail.parser.read_signed_integer
    detail.parser.read_string
    detail.parser.read_timespan
    detail.parser.read_unsigned_integer
    detail.private_thread_pool
    detail.ringbuffer
    detail.ripemd_160
    detail.type_id_list_builder
    detail.unique_function
    dictionary
    dsl
    dynamic_spawn
    error
    expected
    flow.buffer
    flow.concat
    flow.concat_map
    flow.defer
    flow.empty
    flow.fail
    flow.flat_map
    flow.for_each
    flow.generation
    flow.interval
    flow.item_publisher
    flow.merge
    flow.mixed
    flow.never
    flow.observe_on
    flow.prefix_and_tail
    flow.publish
    flow.single
    flow.zip_with
    function_view
    handles
    hash.fnv
    hash.sha1
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
    json_reader
    json_writer
    load_inspector
    logger
    mailbox_element
    message
    message_builder
    message_id
    message_lifetime
    metaprogramming
    mixin.requester
    mixin.sender
    mtl
    node_id
    optional
    or_else
    policy.categorized
    policy.select_all
    policy.select_any
    request_timeout
    response_handle
    response_promise
    result
    save_inspector
    scheduled_actor
    serial_reply
    serialization
    settings
    simple_timeout
    span
    stateful_actor
    string_algorithms
    string_view
    telemetry.collector.prometheus
    telemetry.counter
    telemetry.gauge
    telemetry.histogram
    telemetry.label
    telemetry.metric_registry
    telemetry.timer
    thread_hook
    tracing_data
    type_id_list
    typed_behavior
    typed_message_view
    typed_response_promise
    typed_spawn
    unit
    unordered_flat_map
    uri
    uuid)

if(CAF_ENABLE_TESTING AND CAF_ENABLE_EXCEPTIONS)
  caf_add_test_suites(caf-core-test custom_exception_handler)
endif()
