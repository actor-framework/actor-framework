/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_FWD_HPP
#define CAF_FWD_HPP

#include <memory>
#include <cstdint>

namespace caf {

// -- 1 param templates --------------------------------------------------------

template <class> class param;
template <class> class optional;
template <class> class expected;
template <class> class intrusive_ptr;
template <class> class behavior_type_of;
template <class> class trivial_match_case;
template <class> class weak_intrusive_ptr;
template <class> class typed_continue_helper;

template <class> struct timeout_definition;

// -- 3 param templates --------------------------------------------------------

template <class, class, int> class actor_cast_access;

// -- variadic templates -------------------------------------------------------

template <class...> class result;
template <class...> class delegated;
template <class...> class typed_actor;
template <class...> class typed_actor_pointer;
template <class...> class typed_response_promise;
template <class...> class typed_event_based_actor;

// -- classes ------------------------------------------------------------------

class actor;
class group;
class error;
class message;
class node_id;
class duration;
class behavior;
class resumable;
class actor_addr;
class actor_pool;
class message_id;
class serializer;
class actor_proxy;
class ref_counted;
class local_actor;
class actor_config;
class actor_system;
class deserializer;
class group_module;
class message_view;
class scoped_actor;
class abstract_actor;
class abstract_group;
class actor_registry;
class blocking_actor;
class execution_unit;
class proxy_registry;
class actor_companion;
class continue_helper;
class mailbox_element;
class message_handler;
class scheduled_actor;
class response_promise;
class event_based_actor;
class type_erased_tuple;
class type_erased_value;
class actor_control_block;
class actor_system_config;
class uniform_type_info_map;
class forwarding_actor_proxy;

// -- structs ------------------------------------------------------------------

struct unit_t;
struct exit_msg;
struct down_msg;
struct timeout_msg;
struct group_down_msg;
struct invalid_actor_t;
struct invalid_actor_addr_t;
struct illegal_message_element;
struct prohibit_top_level_spawn_marker;

// -- enums --------------------------------------------------------------------

enum class atom_value : uint64_t;

// -- aliases ------------------------------------------------------------------

using actor_id = uint64_t;

// -- I/O classes --------------------------------------------------------------

namespace io {

class hook;
class broker;
class middleman;

namespace basp {

struct header;

} // namespace basp

} // namespace io

// -- OpenCL classes -----------------------------------------------------------

namespace opencl {

class manager;

} // namespace opencl

// -- scheduler classes --------------------------------------------------------

namespace scheduler {

class abstract_worker;
class abstract_coordinator;

} // namespace scheduler

// -- detail classes -----------------------------------------------------------

namespace detail {

template <class> class type_erased_value_impl;

class disposer;
class message_data;
class group_manager;
class private_thread;
class dynamic_message_data;

} // namespace detail

// -- weak pointer aliases -----------------------------------------------------

using weak_actor_ptr = weak_intrusive_ptr<actor_control_block>;

// -- intrusive pointer aliases ------------------------------------------------

using strong_actor_ptr = intrusive_ptr<actor_control_block>;

// -- unique pointer aliases ---------------------------------------------------

using type_erased_value_ptr = std::unique_ptr<type_erased_value>;
using mailbox_element_ptr = std::unique_ptr<mailbox_element, detail::disposer>;

} // namespace caf

#endif // CAF_FWD_HPP
