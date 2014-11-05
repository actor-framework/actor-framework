/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include <cstdint>

namespace caf {

template <class>
class intrusive_ptr;

// classes
class actor;
class group;
class message;
class channel;
class node_id;
class behavior;
class resumable;
class actor_addr;
class message_id;
class local_actor;
class actor_proxy;
class scoped_actor;
class execution_unit;
class abstract_actor;
class abstract_group;
class blocking_actor;
class message_handler;
class uniform_type_info;
class event_based_actor;

// structs
struct anything;
struct invalid_actor_t;
struct invalid_actor_addr_t;

// enums
enum class atom_value : uint64_t;

// aliases
using actor_id = uint32_t;

// intrusive pointer types
using abstract_group_ptr = intrusive_ptr<abstract_group>;
using actor_proxy_ptr = intrusive_ptr<actor_proxy>;

// functions
template <class T, typename U>
T actor_cast(const U&);

namespace io {
  class broker;
  class middleman;
} // namespace io

namespace scheduler {
  class abstract_worker;
  class abstract_coordinator;
} // namespace scheduler

namespace detail {
  class logging;
  class singletons;
  class message_data;
  class group_manager;
  class actor_registry;
  class uniform_type_info_map;
} // namespace detail

} // namespace caf

#endif // CAF_FWD_HPP
