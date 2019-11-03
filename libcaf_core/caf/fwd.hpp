/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "caf/detail/is_one_of.hpp"
#include "caf/detail/is_primitive_config_value.hpp"
#include "caf/timespan.hpp"

namespace caf {

// clang-format off

// -- 1 param templates --------------------------------------------------------

template <class> class behavior_type_of;
template <class> class dictionary;
template <class> class downstream;
template <class> class expected;
template <class> class intrusive_cow_ptr;
template <class> class intrusive_ptr;
template <class> class optional;
template <class> class param;
template <class> class serializer_impl;
template <class> class span;
template <class> class stream;
template <class> class stream_sink;
template <class> class stream_source;
template <class> class trivial_match_case;
template <class> class weak_intrusive_ptr;

template <class> struct timeout_definition;

// -- 2 param templates --------------------------------------------------------

template <class, class> class stream_stage;

template <class Iterator, class Sentinel = Iterator> struct parser_state;

// -- 3 param templates --------------------------------------------------------

template <class, class, int> class actor_cast_access;

template <class, class, class> class broadcast_downstream_manager;

// -- variadic templates -------------------------------------------------------

template <class...> class cow_tuple;
template <class...> class delegated;
template <class...> class result;
template <class...> class typed_actor;
template <class...> class typed_actor_pointer;
template <class...> class typed_event_based_actor;
template <class...> class typed_response_promise;
template <class...> class variant;

// clang-format on

// -- classes ------------------------------------------------------------------

class abstract_actor;
class abstract_group;
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
class behavior;
class binary_deserializer;
class blocking_actor;
class config_option;
class config_option_adder;
class config_option_set;
class config_value;
class deserializer;
class downstream_manager;
class downstream_manager_base;
class duration;
class error;
class event_based_actor;
class execution_unit;
class forwarding_actor_proxy;
class group;
class group_module;
class inbound_path;
class ipv4_address;
class ipv4_endpoint;
class ipv4_subnet;
class ipv6_address;
class ipv6_endpoint;
class ipv6_subnet;
class local_actor;
class mailbox_element;
class message;
class message_builder;
class message_handler;
class message_id;
class message_view;
class node_id;
class outbound_path;
class proxy_registry;
class ref_counted;
class response_promise;
class resumable;
class scheduled_actor;
class scoped_actor;
class serializer;
class stream_manager;
class string_view;
class tracing_data;
class tracing_data_factory;
class type_erased_tuple;
class type_erased_value;
class uniform_type_info_map;
class uri;
class uri_builder;

// -- templates with default parameters ----------------------------------------

template <class, class = event_based_actor>
class stateful_actor;

// -- structs ------------------------------------------------------------------

struct unit_t;
struct exit_msg;
struct down_msg;
struct timeout_msg;
struct stream_slots;
struct upstream_msg;
struct group_down_msg;
struct downstream_msg;
struct open_stream_msg;
struct invalid_actor_t;
struct invalid_actor_addr_t;
struct illegal_message_element;
struct prohibit_top_level_spawn_marker;

// -- free template functions --------------------------------------------------

template <class T>
config_option make_config_option(string_view category, string_view name,
                                 string_view description);

template <class T>
config_option make_config_option(T& storage, string_view category,
                                 string_view name, string_view description);

// -- enums --------------------------------------------------------------------

enum class atom_value : uint64_t;
enum class byte : uint8_t;
enum class sec : uint8_t;
enum class stream_priority;
enum class invoke_message_result;

// -- aliases ------------------------------------------------------------------

using actor_id = uint64_t;
using binary_serializer = serializer_impl<std::vector<char>>;
using ip_address = ipv6_address;
using ip_endpoint = ipv6_endpoint;
using ip_subnet = ipv6_subnet;
using settings = dictionary<config_value>;
using stream_slot = uint16_t;

// -- functions ----------------------------------------------------------------

/// @relates actor_system_config
const settings& content(const actor_system_config&);

// -- intrusive containers -----------------------------------------------------

namespace intrusive {

enum class task_result;

} // namespace intrusive

// -- marker classes for mixins ------------------------------------------------

namespace mixin {

struct subscriber_base;

} // namespace mixin

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

// -- OpenCL classes -----------------------------------------------------------

namespace opencl {

class manager;

} // namespace opencl

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

template <class>
class type_erased_value_impl;
template <class>
class stream_distribution_tree;

class abstract_worker;
class abstract_worker_hub;
class disposer;
class dynamic_message_data;
class group_manager;
class message_data;
class private_thread;
class uri_impl;

// enable intrusive_ptr<uri_impl> with forward declaration only
void intrusive_ptr_add_ref(const uri_impl*);
void intrusive_ptr_release(const uri_impl*);

// enable intrusive_cow_ptr<dynamic_message_data> with forward declaration only
void intrusive_ptr_add_ref(const dynamic_message_data*);
void intrusive_ptr_release(const dynamic_message_data*);
dynamic_message_data* intrusive_cow_ptr_unshare(dynamic_message_data*&);

} // namespace detail

// -- weak pointer aliases -----------------------------------------------------

using weak_actor_ptr = weak_intrusive_ptr<actor_control_block>;

// -- intrusive pointer aliases ------------------------------------------------

using strong_actor_ptr = intrusive_ptr<actor_control_block>;
using stream_manager_ptr = intrusive_ptr<stream_manager>;

// -- unique pointer aliases ---------------------------------------------------

using mailbox_element_ptr = std::unique_ptr<mailbox_element, detail::disposer>;
using tracing_data_ptr = std::unique_ptr<tracing_data>;
using type_erased_value_ptr = std::unique_ptr<type_erased_value>;

} // namespace caf
