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
#include <set>
#include <string>
#include <utility>

#include "caf/detail/core_export.hpp"
#include "caf/detail/pp.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/fwd.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"

namespace caf {

/// Internal representation of a type ID.
using type_id_t = uint16_t;

/// Maps the type `T` to a globally unique ID.
template <class T>
struct type_id;

/// Convenience alias for `type_id<T>::value`.
/// @relates type_id
template <class T>
constexpr type_id_t type_id_v = type_id<detail::squash_if_int_t<T>>::value;

/// Maps the globally unique ID `V` to a type (inverse to ::type_id).
/// @relates type_id
template <type_id_t V>
struct type_by_id;

/// Convenience alias for `type_by_id<I>::type`.
/// @relates type_by_id
template <type_id_t I>
using type_by_id_t = typename type_by_id<I>::type;

/// Maps the globally unique ID `V` to a type name.
template <type_id_t V>
struct type_name_by_id;

/// Convenience alias for `type_name_by_id<I>::value`.
/// @relates type_name_by_id
template <type_id_t I>
constexpr const char* type_name_by_id_v = type_name_by_id<I>::value;

/// Convenience type that resolves to `type_name_by_id<type_id_v<T>>`.
template <class T>
struct type_name;

/// Convenience specialization that enables generic code to not treat `void`
/// manually.
template <>
struct type_name<void> {
  static constexpr const char* value = "void";
};

/// Convenience alias for `type_name<T>::value`.
/// @relates type_name
template <class T>
constexpr const char* type_name_v = type_name<T>::value;

/// The first type ID not reserved by CAF and its modules.
constexpr type_id_t first_custom_type_id = 200;

} // namespace caf

/// Starts a code block for registering custom types to CAF. Stores the first ID
/// for the project as `caf::${project_name}_first_type_id`. Usually, users
/// should use `caf::first_custom_type_id` as `first_id`. However, this
/// mechanism also enables modules to append IDs to a block of another module or
/// module. If two modules are developed separately to avoid dependencies, they
/// only need to define sufficiently large offsets to guarantee collision-free
/// IDs (CAF supports gaps in the ID space).
#define CAF_BEGIN_TYPE_ID_BLOCK(project_name, first_id)                        \
  namespace caf {                                                              \
  constexpr type_id_t project_name##_type_id_counter_init = __COUNTER__;       \
  constexpr type_id_t project_name##_first_type_id = first_id;                 \
  }

/// Assigns the next free type ID to `fully_qualified_name`.
#define CAF_ADD_TYPE_ID(project_name, fully_qualified_name)                    \
  namespace caf {                                                              \
  template <>                                                                  \
  struct type_id<CAF_PP_EXPAND fully_qualified_name> {                         \
    static constexpr type_id_t value                                           \
      = project_name##_first_type_id                                           \
        + (__COUNTER__ - project_name##_type_id_counter_init - 1);             \
  };                                                                           \
  template <>                                                                  \
  struct type_by_id<type_id<CAF_PP_EXPAND fully_qualified_name>::value> {      \
    using type = CAF_PP_EXPAND fully_qualified_name;                           \
  };                                                                           \
  template <>                                                                  \
  struct type_name<CAF_PP_EXPAND fully_qualified_name> {                       \
    [[maybe_unused]] static constexpr const char* value                        \
      = CAF_PP_STR(CAF_PP_EXPAND fully_qualified_name);                        \
  };                                                                           \
  template <>                                                                  \
  struct type_name_by_id<type_id<CAF_PP_EXPAND fully_qualified_name>::value>   \
    : type_name<CAF_PP_EXPAND fully_qualified_name> {};                        \
  }

/// Creates a new tag type (atom) in the global namespace and assigns the next
/// free type ID to it.
#define CAF_ADD_ATOM_2(project_name, atom_name)                                \
  struct atom_name {};                                                         \
  static constexpr atom_name atom_name##_v = atom_name{};                      \
  [[maybe_unused]] constexpr bool operator==(atom_name, atom_name) {           \
    return true;                                                               \
  }                                                                            \
  [[maybe_unused]] constexpr bool operator!=(atom_name, atom_name) {           \
    return false;                                                              \
  }                                                                            \
  template <class Inspector>                                                   \
  auto inspect(Inspector& f, atom_name&) {                                     \
    return f(caf::meta::type_name(#atom_name));                                \
  }                                                                            \
  CAF_ADD_TYPE_ID(project_name, (atom_name))

/// Creates a new tag type (atom) and assigns the next free type ID to it.
#define CAF_ADD_ATOM_3(project_name, atom_namespace, atom_name)                \
  namespace atom_namespace {                                                   \
  struct atom_name {};                                                         \
  static constexpr atom_name atom_name##_v = atom_name{};                      \
  [[maybe_unused]] constexpr bool operator==(atom_name, atom_name) {           \
    return true;                                                               \
  }                                                                            \
  [[maybe_unused]] constexpr bool operator!=(atom_name, atom_name) {           \
    return false;                                                              \
  }                                                                            \
  template <class Inspector>                                                   \
  auto inspect(Inspector& f, atom_name&) {                                     \
    return f(caf::meta::type_name(#atom_namespace "::" #atom_name));           \
  }                                                                            \
  }                                                                            \
  CAF_ADD_TYPE_ID(project_name, (atom_namespace ::atom_name))

#ifdef CAF_MSVC
#  define CAF_ADD_ATOM(...)                                                    \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_ADD_ATOM_, __VA_ARGS__)(__VA_ARGS__),       \
               CAF_PP_EMPTY())
#else
#  define CAF_ADD_ATOM(...)                                                    \
    CAF_PP_OVERLOAD(CAF_ADD_ATOM_, __VA_ARGS__)(__VA_ARGS__)
#endif

/// Finalizes a code block for registering custom types to CAF. Stores the last
/// type ID used by the project as `caf::${project_name}_last_type_id`.
#define CAF_END_TYPE_ID_BLOCK(project_name)                                    \
  namespace caf {                                                              \
  constexpr type_id_t project_name##_last_type_id                              \
    = project_name##_first_type_id                                             \
      + (__COUNTER__ - project_name##_type_id_counter_init - 2);               \
  struct project_name##_type_ids {                                             \
    static constexpr type_id_t begin = project_name##_first_type_id;           \
    static constexpr type_id_t end = project_name##_last_type_id + 1;         \
  };                                                                           \
  }

CAF_BEGIN_TYPE_ID_BLOCK(builtin, 0)

  // -- C types

  CAF_ADD_TYPE_ID(builtin, (bool))
  CAF_ADD_TYPE_ID(builtin, (double))
  CAF_ADD_TYPE_ID(builtin, (float))
  CAF_ADD_TYPE_ID(builtin, (int16_t))
  CAF_ADD_TYPE_ID(builtin, (int32_t))
  CAF_ADD_TYPE_ID(builtin, (int64_t))
  CAF_ADD_TYPE_ID(builtin, (int8_t))
  CAF_ADD_TYPE_ID(builtin, (long double))
  CAF_ADD_TYPE_ID(builtin, (uint16_t))
  CAF_ADD_TYPE_ID(builtin, (uint32_t))
  CAF_ADD_TYPE_ID(builtin, (uint64_t))
  CAF_ADD_TYPE_ID(builtin, (uint8_t))

  // -- STL types

  CAF_ADD_TYPE_ID(builtin, (std::string))
  CAF_ADD_TYPE_ID(builtin, (std::u16string))
  CAF_ADD_TYPE_ID(builtin, (std::u32string))
  CAF_ADD_TYPE_ID(builtin, (std::set<std::string>))

  // -- CAF types

  CAF_ADD_TYPE_ID(builtin, (caf::actor))
  CAF_ADD_TYPE_ID(builtin, (caf::actor_addr))
  CAF_ADD_TYPE_ID(builtin, (caf::byte_buffer))
  CAF_ADD_TYPE_ID(builtin, (caf::config_value))
  CAF_ADD_TYPE_ID(builtin, (caf::dictionary<caf::config_value>))
  CAF_ADD_TYPE_ID(builtin, (caf::down_msg))
  CAF_ADD_TYPE_ID(builtin, (caf::downstream_msg))
  CAF_ADD_TYPE_ID(builtin, (caf::error))
  CAF_ADD_TYPE_ID(builtin, (caf::exit_msg))
  CAF_ADD_TYPE_ID(builtin, (caf::group))
  CAF_ADD_TYPE_ID(builtin, (caf::group_down_msg))
  CAF_ADD_TYPE_ID(builtin, (caf::message))
  CAF_ADD_TYPE_ID(builtin, (caf::message_id))
  CAF_ADD_TYPE_ID(builtin, (caf::node_id))
  CAF_ADD_TYPE_ID(builtin, (caf::open_stream_msg))
  CAF_ADD_TYPE_ID(builtin, (caf::strong_actor_ptr))
  CAF_ADD_TYPE_ID(builtin, (caf::timeout_msg))
  CAF_ADD_TYPE_ID(builtin, (caf::timespan))
  CAF_ADD_TYPE_ID(builtin, (caf::timestamp))
  CAF_ADD_TYPE_ID(builtin, (caf::unit_t))
  CAF_ADD_TYPE_ID(builtin, (caf::upstream_msg))
  CAF_ADD_TYPE_ID(builtin, (caf::uri))
  CAF_ADD_TYPE_ID(builtin, (caf::weak_actor_ptr))
  CAF_ADD_TYPE_ID(builtin, (std::vector<caf::actor>))
  CAF_ADD_TYPE_ID(builtin, (std::vector<caf::actor_addr>))
  CAF_ADD_TYPE_ID(builtin, (std::vector<caf::config_value>))
  CAF_ADD_TYPE_ID(builtin, (std::vector<caf::strong_actor_ptr>))
  CAF_ADD_TYPE_ID(builtin, (std::vector<caf::weak_actor_ptr>))
  CAF_ADD_TYPE_ID(builtin, (std::vector<std::pair<std::string, message>>));

  // -- predefined atoms

  CAF_ADD_ATOM(builtin, caf, add_atom)
  CAF_ADD_ATOM(builtin, caf, close_atom)
  CAF_ADD_ATOM(builtin, caf, connect_atom)
  CAF_ADD_ATOM(builtin, caf, contact_atom)
  CAF_ADD_ATOM(builtin, caf, delete_atom)
  CAF_ADD_ATOM(builtin, caf, demonitor_atom)
  CAF_ADD_ATOM(builtin, caf, div_atom)
  CAF_ADD_ATOM(builtin, caf, flush_atom)
  CAF_ADD_ATOM(builtin, caf, forward_atom)
  CAF_ADD_ATOM(builtin, caf, get_atom)
  CAF_ADD_ATOM(builtin, caf, idle_atom)
  CAF_ADD_ATOM(builtin, caf, join_atom)
  CAF_ADD_ATOM(builtin, caf, leave_atom)
  CAF_ADD_ATOM(builtin, caf, link_atom)
  CAF_ADD_ATOM(builtin, caf, migrate_atom)
  CAF_ADD_ATOM(builtin, caf, monitor_atom)
  CAF_ADD_ATOM(builtin, caf, mul_atom)
  CAF_ADD_ATOM(builtin, caf, ok_atom)
  CAF_ADD_ATOM(builtin, caf, open_atom)
  CAF_ADD_ATOM(builtin, caf, pending_atom)
  CAF_ADD_ATOM(builtin, caf, ping_atom)
  CAF_ADD_ATOM(builtin, caf, pong_atom)
  CAF_ADD_ATOM(builtin, caf, publish_atom)
  CAF_ADD_ATOM(builtin, caf, publish_udp_atom)
  CAF_ADD_ATOM(builtin, caf, put_atom)
  CAF_ADD_ATOM(builtin, caf, receive_atom)
  CAF_ADD_ATOM(builtin, caf, redirect_atom)
  CAF_ADD_ATOM(builtin, caf, reset_atom)
  CAF_ADD_ATOM(builtin, caf, resolve_atom)
  CAF_ADD_ATOM(builtin, caf, spawn_atom)
  CAF_ADD_ATOM(builtin, caf, stream_atom)
  CAF_ADD_ATOM(builtin, caf, sub_atom)
  CAF_ADD_ATOM(builtin, caf, subscribe_atom)
  CAF_ADD_ATOM(builtin, caf, sys_atom)
  CAF_ADD_ATOM(builtin, caf, tick_atom)
  CAF_ADD_ATOM(builtin, caf, timeout_atom)
  CAF_ADD_ATOM(builtin, caf, unlink_atom)
  CAF_ADD_ATOM(builtin, caf, unpublish_atom)
  CAF_ADD_ATOM(builtin, caf, unpublish_udp_atom)
  CAF_ADD_ATOM(builtin, caf, unsubscribe_atom)
  CAF_ADD_ATOM(builtin, caf, update_atom)
  CAF_ADD_ATOM(builtin, caf, wait_for_atom)

CAF_END_TYPE_ID_BLOCK(builtin)

namespace caf::detail {

static constexpr type_id_t io_module_begin = builtin_type_ids::end;

static constexpr type_id_t io_module_end = io_module_begin + 19;

} // namespace caf
