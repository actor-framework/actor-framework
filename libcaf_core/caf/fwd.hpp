/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

// templates
template <class> class maybe;
template <class> class intrusive_ptr;
template <class> struct actor_cast_access;
template <class> class typed_continue_helper;

// variadic templates
template <class...> class delegated;
template <class...> class typed_response_promise;

// classes
class actor;
class group;
class error;
class message;
class channel;
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
class scoped_actor;
class abstract_actor;
class abstract_group;
class actor_registry;
class blocking_actor;
class execution_unit;
class proxy_registry;
class continue_helper;
class mailbox_element;
class message_handler;
class response_promise;
class event_based_actor;
class binary_serializer;
class binary_deserializer;
class actor_system_config;
class uniform_type_info_map;
class forwarding_actor_proxy;

// structs
struct unit_t;
struct anything;
struct exit_msg;
struct down_msg;
struct timeout_msg;
struct group_down_msg;
struct invalid_actor_t;
struct sync_timeout_msg;
struct invalid_actor_addr_t;
struct illegal_message_element;
struct prohibit_top_level_spawn_marker;

// enums
enum class atom_value : uint64_t;

// aliases
using actor_id = uint32_t;

namespace io {

class broker;
class middleman;

namespace basp {

struct header;

} // namespace basp

} // namespace io

namespace opencl {

class manager;

} // namespace opencl

namespace riac {

class probe;

} // namespace riac

namespace scheduler {

class abstract_worker;
class abstract_coordinator;

} // namespace scheduler

namespace detail {

class disposer;
class message_data;
class group_manager;
class dynamic_message_data;

} // namespace detail

using mailbox_element_ptr = std::unique_ptr<mailbox_element, detail::disposer>;

} // namespace caf

#endif // CAF_FWD_HPP
