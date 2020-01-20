/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
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
#include <utility>

#include "caf/fwd.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/type_nr.hpp"

namespace caf {

/// Maps the type `T` to a globally unique 16-bit ID.
template <class T>
struct type_id;

/// Convenience alias for `type_id<T>::value`.
/// @relates type_id
template <class T>
constexpr uint16_t type_id_v = type_id<T>::value;

/// Maps the globally unique ID `V` to a type (inverse to ::type_id).
/// @relates type_id
template <uint16_t V>
struct type_by_id;

/// Convenience alias for `type_by_id<I>::type`.
/// @relates type_by_id
template <uint16_t I>
using type_by_id_t = typename type_by_id<I>::type;

/// Maps the globally unique ID `V` to a type name.
template <uint16_t V>
struct type_name_by_id;

/// Convenience alias for `type_name_by_id<I>::value`.
/// @relates type_name_by_id
template <uint16_t I>
constexpr const char* type_name_by_id_v = type_name_by_id<I>::value;

/// The first type ID not reserved by CAF and its modules.
constexpr uint16_t first_custom_type_id = 200;

} // namespace caf

/// Starts a code block for registering custom types to CAF. Stores the first ID
/// for the project as `caf::${project_name}_first_type_id`. Unless the project
/// appends to an ID block of another project, users should use
/// `caf::first_custom_type_id` as `first_id`.
#define CAF_BEGIN_TYPE_ID_BLOCK(project_name, first_id)                        \
  namespace caf {                                                              \
  constexpr uint16_t project_name##_type_id_counter_init = __COUNTER__;        \
  constexpr uint16_t project_name##_first_type_id = first_id;                  \
  }

/// Assigns the next free type ID to `fully_qualified_name`.
#define CAF_ADD_TYPE_ID(project_name, fully_qualified_name)                    \
  namespace caf {                                                              \
  template <>                                                                  \
  struct type_id<fully_qualified_name> {                                       \
    static constexpr uint16_t value                                            \
      = project_name##_first_type_id                                           \
        + (__COUNTER__ - project_name##_type_id_counter_init - 1);             \
  };                                                                           \
  template <>                                                                  \
  struct type_by_id<type_id<fully_qualified_name>::value> {                    \
    using type = fully_qualified_name;                                         \
  };                                                                           \
  template <>                                                                  \
  struct type_name_by_id<type_id<fully_qualified_name>::value> {               \
    static constexpr const char* value = #fully_qualified_name;                \
  };                                                                           \
  }

/// Creates a new tag type (atom) and assigns the next free type ID to it.
#define CAF_ADD_ATOM(project_name, atom_namespace, atom_name)                  \
  namespace atom_namespace {                                                   \
  struct atom_name {};                                                         \
  static constexpr atom_name atom_name##_v = atom_name{};                      \
  template <class Inspector>                                                   \
  auto inspect(Inspector& f, atom_name&) {                                     \
    return f(caf::meta::type_name(#atom_namespace #atom_name));                \
  }                                                                            \
  }                                                                            \
  CAF_ADD_TYPE_ID(project_name, atom_namespace##atom_name)

/// Finalizes a code block for registering custom types to CAF. Stores the last
/// type ID used by the project as `caf::${project_name}_last_type_id`.
#define CAF_END_TYPE_ID_BLOCK(project_name)                                    \
  namespace caf {                                                              \
  constexpr uint16_t project_name##_last_type_id                               \
    = project_name##_first_type_id                                             \
      + (__COUNTER__ - project_name##_type_id_counter_init - 2);               \
  struct project_name##_type_ids {                                             \
    static constexpr uint16_t first = project_name##_first_type_id;            \
    static constexpr uint16_t last = project_name##_last_type_id;              \
  };                                                                           \
  }

CAF_BEGIN_TYPE_ID_BLOCK(caf_core, 0)

  // -- C types

  CAF_ADD_TYPE_ID(caf_core, bool)
  CAF_ADD_TYPE_ID(caf_core, double)
  CAF_ADD_TYPE_ID(caf_core, float)
  CAF_ADD_TYPE_ID(caf_core, int16_t)
  CAF_ADD_TYPE_ID(caf_core, int32_t)
  CAF_ADD_TYPE_ID(caf_core, int64_t)
  CAF_ADD_TYPE_ID(caf_core, int8_t)
  CAF_ADD_TYPE_ID(caf_core, long double)
  CAF_ADD_TYPE_ID(caf_core, uint16_t)
  CAF_ADD_TYPE_ID(caf_core, uint32_t)
  CAF_ADD_TYPE_ID(caf_core, uint64_t)
  CAF_ADD_TYPE_ID(caf_core, uint8_t)

  // -- STL types

  CAF_ADD_TYPE_ID(caf_core, std::string)
  CAF_ADD_TYPE_ID(caf_core, std::u16string)
  CAF_ADD_TYPE_ID(caf_core, std::u32string)
  CAF_ADD_TYPE_ID(caf_core, std::set<std::string>)

  // -- CAF types

  CAF_ADD_TYPE_ID(caf_core, caf::actor)
  CAF_ADD_TYPE_ID(caf_core, caf::actor_addr)
  CAF_ADD_TYPE_ID(caf_core, caf::byte_buffer)
  CAF_ADD_TYPE_ID(caf_core, caf::config_value)
  CAF_ADD_TYPE_ID(caf_core, caf::down_msg)
  CAF_ADD_TYPE_ID(caf_core, caf::downstream_msg)
  CAF_ADD_TYPE_ID(caf_core, caf::error)
  CAF_ADD_TYPE_ID(caf_core, caf::exit_msg)
  CAF_ADD_TYPE_ID(caf_core, caf::group)
  CAF_ADD_TYPE_ID(caf_core, caf::group_down_msg)
  CAF_ADD_TYPE_ID(caf_core, caf::message)
  CAF_ADD_TYPE_ID(caf_core, caf::message_id)
  CAF_ADD_TYPE_ID(caf_core, caf::node_id)
  CAF_ADD_TYPE_ID(caf_core, caf::open_stream_msg)
  CAF_ADD_TYPE_ID(caf_core, caf::strong_actor_ptr)
  CAF_ADD_TYPE_ID(caf_core, caf::timeout_msg)
  CAF_ADD_TYPE_ID(caf_core, caf::timespan)
  CAF_ADD_TYPE_ID(caf_core, caf::timestamp)
  CAF_ADD_TYPE_ID(caf_core, caf::unit_t)
  CAF_ADD_TYPE_ID(caf_core, caf::upstream_msg)
  CAF_ADD_TYPE_ID(caf_core, caf::weak_actor_ptr)
  CAF_ADD_TYPE_ID(caf_core, std::vector<caf::actor>)
  CAF_ADD_TYPE_ID(caf_core, std::vector<caf::actor_addr>)

  // -- predefined atoms

  CAF_ADD_TYPE_ID(caf_core, add_atom)
  CAF_ADD_TYPE_ID(caf_core, close_atom)
  CAF_ADD_TYPE_ID(caf_core, connect_atom)
  CAF_ADD_TYPE_ID(caf_core, contact_atom)
  CAF_ADD_TYPE_ID(caf_core, delete_atom)
  CAF_ADD_TYPE_ID(caf_core, demonitor_atom)
  CAF_ADD_TYPE_ID(caf_core, div_atom)
  CAF_ADD_TYPE_ID(caf_core, flush_atom)
  CAF_ADD_TYPE_ID(caf_core, forward_atom)
  CAF_ADD_TYPE_ID(caf_core, get_atom)
  CAF_ADD_TYPE_ID(caf_core, idle_atom)
  CAF_ADD_TYPE_ID(caf_core, join_atom)
  CAF_ADD_TYPE_ID(caf_core, leave_atom)
  CAF_ADD_TYPE_ID(caf_core, link_atom)
  CAF_ADD_TYPE_ID(caf_core, migrate_atom)
  CAF_ADD_TYPE_ID(caf_core, monitor_atom)
  CAF_ADD_TYPE_ID(caf_core, mul_atom)
  CAF_ADD_TYPE_ID(caf_core, ok_atom)
  CAF_ADD_TYPE_ID(caf_core, open_atom)
  CAF_ADD_TYPE_ID(caf_core, pending_atom)
  CAF_ADD_TYPE_ID(caf_core, ping_atom)
  CAF_ADD_TYPE_ID(caf_core, pong_atom)
  CAF_ADD_TYPE_ID(caf_core, publish_atom)
  CAF_ADD_TYPE_ID(caf_core, publish_udp_atom)
  CAF_ADD_TYPE_ID(caf_core, put_atom)
  CAF_ADD_TYPE_ID(caf_core, receive_atom)
  CAF_ADD_TYPE_ID(caf_core, redirect_atom)
  CAF_ADD_TYPE_ID(caf_core, reset_atom)
  CAF_ADD_TYPE_ID(caf_core, resolve_atom)
  CAF_ADD_TYPE_ID(caf_core, spawn_atom)
  CAF_ADD_TYPE_ID(caf_core, stream_atom)
  CAF_ADD_TYPE_ID(caf_core, sub_atom)
  CAF_ADD_TYPE_ID(caf_core, subscribe_atom)
  CAF_ADD_TYPE_ID(caf_core, sys_atom)
  CAF_ADD_TYPE_ID(caf_core, tick_atom)
  CAF_ADD_TYPE_ID(caf_core, timeout_atom)
  CAF_ADD_TYPE_ID(caf_core, unlink_atom)
  CAF_ADD_TYPE_ID(caf_core, unpublish_atom)
  CAF_ADD_TYPE_ID(caf_core, unpublish_udp_atom)
  CAF_ADD_TYPE_ID(caf_core, unsubscribe_atom)
  CAF_ADD_TYPE_ID(caf_core, update_atom)
  CAF_ADD_TYPE_ID(caf_core, wait_for_atom)

  // TODO: remove atoms from type_nr.hpp and uncomment this block
  // CAF_ADD_ATOM(caf_core, caf, add_atom)
  // CAF_ADD_ATOM(caf_core, caf, close_atom)
  // CAF_ADD_ATOM(caf_core, caf, connect_atom)
  // CAF_ADD_ATOM(caf_core, caf, contact_atom)
  // CAF_ADD_ATOM(caf_core, caf, delete_atom)
  // CAF_ADD_ATOM(caf_core, caf, demonitor_atom)
  // CAF_ADD_ATOM(caf_core, caf, div_atom)
  // CAF_ADD_ATOM(caf_core, caf, flush_atom)
  // CAF_ADD_ATOM(caf_core, caf, forward_atom)
  // CAF_ADD_ATOM(caf_core, caf, get_atom)
  // CAF_ADD_ATOM(caf_core, caf, idle_atom)
  // CAF_ADD_ATOM(caf_core, caf, join_atom)
  // CAF_ADD_ATOM(caf_core, caf, leave_atom)
  // CAF_ADD_ATOM(caf_core, caf, link_atom)
  // CAF_ADD_ATOM(caf_core, caf, migrate_atom)
  // CAF_ADD_ATOM(caf_core, caf, monitor_atom)
  // CAF_ADD_ATOM(caf_core, caf, mul_atom)
  // CAF_ADD_ATOM(caf_core, caf, ok_atom)
  // CAF_ADD_ATOM(caf_core, caf, open_atom)
  // CAF_ADD_ATOM(caf_core, caf, pending_atom)
  // CAF_ADD_ATOM(caf_core, caf, ping_atom)
  // CAF_ADD_ATOM(caf_core, caf, pong_atom)
  // CAF_ADD_ATOM(caf_core, caf, publish_atom)
  // CAF_ADD_ATOM(caf_core, caf, publish_udp_atom)
  // CAF_ADD_ATOM(caf_core, caf, put_atom)
  // CAF_ADD_ATOM(caf_core, caf, receive_atom)
  // CAF_ADD_ATOM(caf_core, caf, redirect_atom)
  // CAF_ADD_ATOM(caf_core, caf, reset_atom)
  // CAF_ADD_ATOM(caf_core, caf, resolve_atom)
  // CAF_ADD_ATOM(caf_core, caf, spawn_atom)
  // CAF_ADD_ATOM(caf_core, caf, stream_atom)
  // CAF_ADD_ATOM(caf_core, caf, sub_atom)
  // CAF_ADD_ATOM(caf_core, caf, subscribe_atom)
  // CAF_ADD_ATOM(caf_core, caf, sys_atom)
  // CAF_ADD_ATOM(caf_core, caf, tick_atom)
  // CAF_ADD_ATOM(caf_core, caf, timeout_atom)
  // CAF_ADD_ATOM(caf_core, caf, unlink_atom)
  // CAF_ADD_ATOM(caf_core, caf, unpublish_atom)
  // CAF_ADD_ATOM(caf_core, caf, unpublish_udp_atom)
  // CAF_ADD_ATOM(caf_core, caf, unsubscribe_atom)
  // CAF_ADD_ATOM(caf_core, caf, update_atom)
  // CAF_ADD_ATOM(caf_core, caf, wait_for_atom)

CAF_END_TYPE_ID_BLOCK(caf_core)
