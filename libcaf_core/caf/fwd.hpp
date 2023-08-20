// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace caf {

// clang-format off

// -- 1 param templates --------------------------------------------------------

template <class> class [[deprecated ("use std::optional instead")]] optional;
template <class> class [[nodiscard]] error_code;
template <class> class basic_cow_string;
template <class> class behavior_type_of;
template <class> class callback;
template <class> class cow_vector;
template <class> class dictionary;
template <class> class expected;
template <class> class intrusive_cow_ptr;
template <class> class intrusive_ptr;
template <class> class param;
template <class> class span;
template <class> class typed_stream;
template <class> class weak_intrusive_ptr;

template <class> struct inspector_access;
template <class> struct timeout_definition;
template <class> struct type_id;

template <uint16_t> struct type_by_id;
template <uint16_t> struct type_name_by_id;

// -- 2 param templates --------------------------------------------------------

template <class Iterator, class Sentinel = Iterator> struct parser_state;

// -- 3 param templates --------------------------------------------------------

template <class, class, int> class actor_cast_access;

template <class K, class V, class = std::allocator<std::pair<K, V>>>
class unordered_flat_map;

// -- variadic templates -------------------------------------------------------

template <class...> class const_typed_message_view;
template <class...> class cow_tuple;
template <class...> class delegated;
template <class...> class result;
template <class...> class typed_actor;
template <class...> class typed_actor_pointer;
template <class...> class typed_actor_view;
template <class...> class typed_event_based_actor;
template <class...> class typed_message_view;
template <class...> class typed_response_promise;

// clang-format on

// -- classes ------------------------------------------------------------------

class [[deprecated("use std::string_view instead")]] string_view;
class [[nodiscard]] error;
class abstract_actor;
class abstract_group;
class abstract_mailbox;
class action;
class actor;
class actor_addr;
class actor_clock;
class actor_companion;
class actor_config;
class actor_control_block;
class actor_pool;
class actor_profiler;
class actor_proxy;
class actor_registry;
class actor_system;
class actor_system_config;
class attachable;
class behavior;
class binary_deserializer;
class binary_serializer;
class blocking_actor;
class config_option;
class config_option_adder;
class config_option_set;
class config_value;
class deserializer;
class disposable;
class event_based_actor;
class execution_unit;
class forwarding_actor_proxy;
class group;
class group_module;
class hashed_node_id;
class ipv4_address;
class ipv4_endpoint;
class ipv4_subnet;
class ipv6_address;
class ipv6_endpoint;
class ipv6_subnet;
class json_array;
class json_object;
class json_reader;
class json_value;
class json_writer;
class local_actor;
class mailbox_element;
class message;
class message_builder;
class message_handler;
class message_id;
class node_id;
class node_id_data;
class proxy_registry;
class ref_counted;
class response_promise;
class resumable;
class scheduled_actor;
class scoped_actor;
class serializer;
class skip_t;
class skippable_result;
class stream;
class tracing_data;
class tracing_data_factory;
class type_id_list;
class uri;
class uri_builder;
class uuid;

// -- templates with default parameters ----------------------------------------

template <class, class = event_based_actor>
class stateful_actor;

// -- structs ------------------------------------------------------------------

struct down_msg;
struct exit_msg;
struct group_down_msg;
struct illegal_message_element;
struct invalid_actor_addr_t;
struct invalid_actor_t;
struct node_down_msg;
struct none_t;
struct prohibit_top_level_spawn_marker;
struct stream_abort_msg;
struct stream_ack_msg;
struct stream_batch_msg;
struct stream_cancel_msg;
struct stream_close_msg;
struct stream_demand_msg;
struct stream_open_msg;
struct unit_t;

// -- free template functions --------------------------------------------------

template <class T>
config_option make_config_option(std::string_view category,
                                 std::string_view name,
                                 std::string_view description);

template <class T>
config_option make_config_option(T& storage, std::string_view category,
                                 std::string_view name,
                                 std::string_view description);

// -- enums --------------------------------------------------------------------

enum class exit_reason : uint8_t;
enum class invoke_message_result;
enum class pec : uint8_t;
enum class sec : uint8_t;
enum class thread_owner;

// -- aliases ------------------------------------------------------------------

using actor_id = uint64_t;
using byte [[deprecated("use std::byte instead")]] = std::byte;
using byte_buffer = std::vector<std::byte>;
using byte_span = span<std::byte>;
using const_byte_span = span<const std::byte>;
using cow_string = basic_cow_string<char>;
using cow_u16string = basic_cow_string<char16_t>;
using cow_u32string = basic_cow_string<char32_t>;
using ip_address = ipv6_address;
using ip_endpoint = ipv6_endpoint;
using ip_subnet = ipv6_subnet;
using settings = dictionary<config_value>;
using type_id_t = uint16_t;

// -- functions ----------------------------------------------------------------

/// @relates actor_system_config
CAF_CORE_EXPORT const settings& content(const actor_system_config&);

// -- hash inspectors ----------------------------------------------------------

namespace hash {

template <class>
class fnv;

} // namespace hash

// -- marker classes for mixins ------------------------------------------------

namespace mixin {

struct subscriber_base;

} // namespace mixin

// -- telemetry API ------------------------------------------------------------

namespace telemetry {

class component;
class dbl_gauge;
class int_gauge;
class label;
class label_view;
class metric;
class metric_family;
class metric_registry;
class timer;

enum class metric_type : uint8_t;

template <class ValueType>
class counter;

template <class ValueType>
class histogram;

template <class Type>
class metric_family_impl;

template <class Type>
class metric_impl;

using dbl_counter = counter<double>;
using dbl_histogram = histogram<double>;
using int_counter = counter<int64_t>;
using int_histogram = histogram<int64_t>;

using dbl_counter_family = metric_family_impl<dbl_counter>;
using dbl_histogram_family = metric_family_impl<dbl_histogram>;
using dbl_gauge_family = metric_family_impl<dbl_gauge>;
using int_counter_family = metric_family_impl<int_counter>;
using int_histogram_family = metric_family_impl<int_histogram>;
using int_gauge_family = metric_family_impl<int_gauge>;

} // namespace telemetry

namespace detail {

class mailbox_factory;

template <class>
struct gauge_oracle;

template <>
struct gauge_oracle<double> {
  using type = telemetry::dbl_gauge;
};

template <>
struct gauge_oracle<int64_t> {
  using type = telemetry::int_gauge;
};

/// Convenience alias for `detail::gauge_oracle<ValueType>::type`.
template <class ValueType>
using gauge_oracle_t = typename gauge_oracle<ValueType>::type;

} // namespace detail

namespace telemetry {

template <class ValueType>
using gauge = detail::gauge_oracle_t<ValueType>;

} // namespace telemetry

// -- I/O classes --------------------------------------------------------------

namespace io {

class hook;
class broker;
class middleman;

namespace basp {

struct header;

} // namespace basp

} // namespace io

// -- networking classes -------------------------------------------------------

namespace net {

class middleman;

} // namespace net

// -- scheduler classes --------------------------------------------------------

namespace scheduler {

class abstract_worker;
class test_coordinator;
class abstract_coordinator;

} // namespace scheduler

// -- OpenSSL classes ----------------------------------------------------------

namespace openssl {

class manager;

} // namespace openssl

// -- detail classes -----------------------------------------------------------

namespace detail {

class abstract_worker;
class abstract_worker_hub;
class disposer;
class dynamic_message_data;
class group_manager;
class message_data;
class private_thread;
class stream_bridge;
class stream_bridge_sub;

struct meta_object;

// enable intrusive_cow_ptr<dynamic_message_data> with forward declaration only
CAF_CORE_EXPORT void intrusive_ptr_add_ref(const dynamic_message_data*);
CAF_CORE_EXPORT void intrusive_ptr_release(const dynamic_message_data*);
CAF_CORE_EXPORT dynamic_message_data*
intrusive_cow_ptr_unshare(dynamic_message_data*&);

using global_meta_objects_guard_type = intrusive_ptr<ref_counted>;

} // namespace detail

// -- weak pointer aliases -----------------------------------------------------

using weak_actor_ptr = weak_intrusive_ptr<actor_control_block>;

// -- intrusive pointer aliases ------------------------------------------------

using group_module_ptr = intrusive_ptr<group_module>;
using strong_actor_ptr = intrusive_ptr<actor_control_block>;

// -- unique pointer aliases ---------------------------------------------------

using mailbox_element_ptr = std::unique_ptr<mailbox_element>;
using tracing_data_ptr = std::unique_ptr<tracing_data>;

// -- shared pointer aliases ---------------------------------------------------

using shared_action_ptr = std::shared_ptr<callback<void()>>;

} // namespace caf
