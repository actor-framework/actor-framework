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
#include "caf/detail/is_complete.hpp"
#include "caf/detail/pp.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/fwd.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/string_view.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"

namespace caf {

/// Internal representation of a type ID.
using type_id_t = uint16_t;

/// Special value equal to the greatest possible value for `type_id_t`.
/// Generally indicates that no type ID for a given type exists.
constexpr type_id_t invalid_type_id = 65535;

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
constexpr string_view type_name_by_id_v = type_name_by_id<I>::value;

/// Convenience type that resolves to `type_name_by_id<type_id_v<T>>`.
template <class T>
struct type_name;

/// Convenience specialization that enables generic code to not treat `void`
/// manually.
template <>
struct type_name<void> {
  static constexpr string_view value = "void";
};

/// Convenience alias for `type_name<T>::value`.
/// @relates type_name
template <class T>
constexpr string_view type_name_v = type_name<T>::value;

/// The first type ID not reserved by CAF and its modules.
constexpr type_id_t first_custom_type_id = 200;

/// Checks whether `type_id` is specialized for `T`.
template <class T>
constexpr bool has_type_id_v = detail::is_complete<type_id<T>>;

// TODO: remove with CAF 0.19 (this exists for compatibility with CAF 0.17).
template <class T>
struct has_type_id {
  static constexpr bool value = detail::is_complete<type_id<T>>;
};

/// Returns `type_name_v<T>` if available, "anonymous" otherwise.
template <class T>
string_view type_name_or_anonymous() {
  if constexpr (detail::is_complete<type_name<T>>)
    return type_name<T>::value;
  else
    return "anonymous";
}

/// Returns the type name of given `type` or an empty string if `type` is an
/// invalid ID.
CAF_CORE_EXPORT string_view query_type_name(type_id_t type);

/// Returns the type of given `name` or `invalid_type_id` if no type matches.
CAF_CORE_EXPORT type_id_t query_type_id(string_view name);

} // namespace caf

/// Starts a code block for registering custom types to CAF. Stores the first ID
/// for the project as `caf::id_block::${project_name}_first_type_id`. Usually,
/// users should use `caf::first_custom_type_id` as `first_id`. However, this
/// mechanism also enables projects to append IDs to a block of another project.
/// If two projects are developed separately to avoid dependencies, they only
/// need to define sufficiently large offsets to guarantee collision-free IDs.
/// CAF supports gaps in the ID space.
///
/// @note CAF reserves all names with the suffix `_module`. For example, core
///       module uses the project name `core_module`.
#define CAF_BEGIN_TYPE_ID_BLOCK(project_name, first_id)                        \
  namespace caf::id_block {                                                    \
  constexpr type_id_t project_name##_type_id_counter_init = __COUNTER__;       \
  constexpr type_id_t project_name##_first_type_id = first_id;                 \
  }

#ifdef CAF_MSVC
#  define CAF_DETAIL_NEXT_TYPE_ID(project_name, fully_qualified_name)          \
    template <>                                                                \
    struct type_id<CAF_PP_EXPAND fully_qualified_name> {                       \
      static constexpr type_id_t value                                         \
        = id_block::project_name##_first_type_id                               \
          + (CAF_PP_CAT(CAF_PP_COUNTER, ())                                    \
             - id_block::project_name##_type_id_counter_init - 1);             \
    };
#else
#  define CAF_DETAIL_NEXT_TYPE_ID(project_name, fully_qualified_name)          \
    template <>                                                                \
    struct type_id<CAF_PP_EXPAND fully_qualified_name> {                       \
      static constexpr type_id_t value                                         \
        = id_block::project_name##_first_type_id                               \
          + (__COUNTER__ - id_block::project_name##_type_id_counter_init - 1); \
    };
#endif

#define CAF_ADD_TYPE_ID_2(project_name, fully_qualified_name)                  \
  namespace caf {                                                              \
  CAF_DETAIL_NEXT_TYPE_ID(project_name, fully_qualified_name)                  \
  template <>                                                                  \
  struct type_by_id<type_id<CAF_PP_EXPAND fully_qualified_name>::value> {      \
    using type = CAF_PP_EXPAND fully_qualified_name;                           \
  };                                                                           \
  template <>                                                                  \
  struct type_name<CAF_PP_EXPAND fully_qualified_name> {                       \
    static constexpr string_view value                                         \
      = CAF_PP_STR(CAF_PP_EXPAND fully_qualified_name);                        \
  };                                                                           \
  template <>                                                                  \
  struct type_name_by_id<type_id<CAF_PP_EXPAND fully_qualified_name>::value>   \
    : type_name<CAF_PP_EXPAND fully_qualified_name> {};                        \
  }

#define CAF_ADD_TYPE_ID_3(project_name, fully_qualified_name, user_type_name)  \
  namespace caf {                                                              \
  CAF_DETAIL_NEXT_TYPE_ID(project_name, fully_qualified_name)                  \
  template <>                                                                  \
  struct type_by_id<type_id<CAF_PP_EXPAND fully_qualified_name>::value> {      \
    using type = CAF_PP_EXPAND fully_qualified_name;                           \
  };                                                                           \
  template <>                                                                  \
  struct type_name<CAF_PP_EXPAND fully_qualified_name> {                       \
    static constexpr string_view value = user_type_name;                       \
  };                                                                           \
  template <>                                                                  \
  struct type_name_by_id<type_id<CAF_PP_EXPAND fully_qualified_name>::value>   \
    : type_name<CAF_PP_EXPAND fully_qualified_name> {};                        \
  }

/// @def CAF_ADD_TYPE_ID(project_name, fully_qualified_name, user_type_name)
/// Assigns the next free type ID to `fully_qualified_name`.
/// @param project_name User-defined name for type ID block.
/// @param fully_qualified_name Name of the type enclosed in parens and
///                             including namespaces, e.g., `(std::string)`.
/// @param user_type_name Optional parameter. If present, defines the content of
///                       `caf::type_name`. Defaults to `fully_qualified_name`.
#ifdef CAF_MSVC
#  define CAF_ADD_TYPE_ID(...)                                                 \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_ADD_TYPE_ID_, __VA_ARGS__)(__VA_ARGS__),    \
               CAF_PP_EMPTY())
#else
#  define CAF_ADD_TYPE_ID(...)                                                 \
    CAF_PP_OVERLOAD(CAF_ADD_TYPE_ID_, __VA_ARGS__)(__VA_ARGS__)
#endif

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
  bool inspect(Inspector& f, atom_name& x) {                                   \
    return f.object(x).fields();                                               \
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
  bool inspect(Inspector& f, atom_name& x) {                                   \
    return f.object(x).fields();                                               \
  }                                                                            \
  }                                                                            \
  CAF_ADD_TYPE_ID(project_name, (atom_namespace::atom_name))

/// Creates a new tag type (atom) and assigns the next free type ID to it.
#define CAF_ADD_ATOM_4(project_name, atom_namespace, atom_name, atom_text)     \
  namespace atom_namespace {                                                   \
  struct atom_name {};                                                         \
  static constexpr atom_name atom_name##_v = atom_name{};                      \
  [[maybe_unused]] constexpr bool operator==(atom_name, atom_name) {           \
    return true;                                                               \
  }                                                                            \
  [[maybe_unused]] constexpr bool operator!=(atom_name, atom_name) {           \
    return false;                                                              \
  }                                                                            \
  inline std::string to_string(atom_name) {                                    \
    return atom_text;                                                          \
  }                                                                            \
  template <class Inspector>                                                   \
  auto inspect(Inspector& f, atom_name& x) {                                   \
    return f.object(x).fields();                                               \
  }                                                                            \
  }                                                                            \
  CAF_ADD_TYPE_ID(project_name, (atom_namespace::atom_name))

#ifdef CAF_MSVC
#  define CAF_ADD_ATOM(...)                                                    \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_ADD_ATOM_, __VA_ARGS__)(__VA_ARGS__),       \
               CAF_PP_EMPTY())
#else
#  define CAF_ADD_ATOM(...)                                                    \
    CAF_PP_OVERLOAD(CAF_ADD_ATOM_, __VA_ARGS__)(__VA_ARGS__)
#endif

/// Finalizes a code block for registering custom types to CAF. Defines a struct
/// `caf::type_id::${project_name}` with two static members `begin` and `end`.
/// The former stores the first assigned type ID. The latter stores the last
/// assigned type ID + 1.
#define CAF_END_TYPE_ID_BLOCK(project_name)                                    \
  namespace caf::id_block {                                                    \
  constexpr type_id_t project_name##_last_type_id                              \
    = project_name##_first_type_id                                             \
      + (__COUNTER__ - project_name##_type_id_counter_init - 2);               \
  struct project_name {                                                        \
    static constexpr type_id_t begin = project_name##_first_type_id;           \
    static constexpr type_id_t end = project_name##_last_type_id + 1;          \
  };                                                                           \
  }

CAF_BEGIN_TYPE_ID_BLOCK(core_module, 0)

  // -- C types

  CAF_ADD_TYPE_ID(core_module, (bool) )
  CAF_ADD_TYPE_ID(core_module, (double) )
  CAF_ADD_TYPE_ID(core_module, (float) )
  CAF_ADD_TYPE_ID(core_module, (int16_t))
  CAF_ADD_TYPE_ID(core_module, (int32_t))
  CAF_ADD_TYPE_ID(core_module, (int64_t))
  CAF_ADD_TYPE_ID(core_module, (int8_t))
  CAF_ADD_TYPE_ID(core_module, (long double), "ldouble")
  CAF_ADD_TYPE_ID(core_module, (uint16_t))
  CAF_ADD_TYPE_ID(core_module, (uint32_t))
  CAF_ADD_TYPE_ID(core_module, (uint64_t))
  CAF_ADD_TYPE_ID(core_module, (uint8_t))

  // -- STL types

  CAF_ADD_TYPE_ID(core_module, (std::string))
  CAF_ADD_TYPE_ID(core_module, (std::u16string))
  CAF_ADD_TYPE_ID(core_module, (std::u32string))
  CAF_ADD_TYPE_ID(core_module, (std::set<std::string>) )

  // -- CAF types

  CAF_ADD_TYPE_ID(core_module, (caf::actor))
  CAF_ADD_TYPE_ID(core_module, (caf::actor_addr))
  CAF_ADD_TYPE_ID(core_module, (caf::byte_buffer))
  CAF_ADD_TYPE_ID(core_module, (caf::config_value))
  CAF_ADD_TYPE_ID(core_module, (caf::dictionary<caf::config_value>) )
  CAF_ADD_TYPE_ID(core_module, (caf::down_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::downstream_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::downstream_msg_batch))
  CAF_ADD_TYPE_ID(core_module, (caf::downstream_msg_close))
  CAF_ADD_TYPE_ID(core_module, (caf::downstream_msg_forced_close))
  CAF_ADD_TYPE_ID(core_module, (caf::error))
  CAF_ADD_TYPE_ID(core_module, (caf::exit_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::exit_reason))
  CAF_ADD_TYPE_ID(core_module, (caf::group))
  CAF_ADD_TYPE_ID(core_module, (caf::group_down_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::hashed_node_id))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv4_address))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv4_endpoint))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv4_subnet))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv6_address))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv6_endpoint))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv6_subnet))
  CAF_ADD_TYPE_ID(core_module, (caf::message))
  CAF_ADD_TYPE_ID(core_module, (caf::message_id))
  CAF_ADD_TYPE_ID(core_module, (caf::node_down_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::node_id))
  CAF_ADD_TYPE_ID(core_module, (caf::none_t))
  CAF_ADD_TYPE_ID(core_module, (caf::open_stream_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::pec))
  CAF_ADD_TYPE_ID(core_module, (caf::sec))
  CAF_ADD_TYPE_ID(core_module, (caf::stream_slots))
  CAF_ADD_TYPE_ID(core_module, (caf::strong_actor_ptr))
  CAF_ADD_TYPE_ID(core_module, (caf::timeout_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::timespan))
  CAF_ADD_TYPE_ID(core_module, (caf::timestamp))
  CAF_ADD_TYPE_ID(core_module, (caf::unit_t))
  CAF_ADD_TYPE_ID(core_module, (caf::upstream_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::upstream_msg_ack_batch))
  CAF_ADD_TYPE_ID(core_module, (caf::upstream_msg_ack_open))
  CAF_ADD_TYPE_ID(core_module, (caf::upstream_msg_drop))
  CAF_ADD_TYPE_ID(core_module, (caf::upstream_msg_forced_drop))
  CAF_ADD_TYPE_ID(core_module, (caf::uri))
  CAF_ADD_TYPE_ID(core_module, (caf::weak_actor_ptr))
  CAF_ADD_TYPE_ID(core_module, (std::vector<caf::actor>) )
  CAF_ADD_TYPE_ID(core_module, (std::vector<caf::actor_addr>) )
  CAF_ADD_TYPE_ID(core_module, (std::vector<caf::config_value>) )
  CAF_ADD_TYPE_ID(core_module, (std::vector<caf::strong_actor_ptr>) )
  CAF_ADD_TYPE_ID(core_module, (std::vector<caf::weak_actor_ptr>) )
  CAF_ADD_TYPE_ID(core_module, (std::vector<std::pair<std::string, message>>) )

  // -- predefined atoms

  CAF_ADD_ATOM(core_module, caf, add_atom)
  CAF_ADD_ATOM(core_module, caf, close_atom)
  CAF_ADD_ATOM(core_module, caf, connect_atom)
  CAF_ADD_ATOM(core_module, caf, contact_atom)
  CAF_ADD_ATOM(core_module, caf, delete_atom)
  CAF_ADD_ATOM(core_module, caf, demonitor_atom)
  CAF_ADD_ATOM(core_module, caf, div_atom)
  CAF_ADD_ATOM(core_module, caf, flush_atom)
  CAF_ADD_ATOM(core_module, caf, forward_atom)
  CAF_ADD_ATOM(core_module, caf, get_atom)
  CAF_ADD_ATOM(core_module, caf, group_atom)
  CAF_ADD_ATOM(core_module, caf, idle_atom)
  CAF_ADD_ATOM(core_module, caf, join_atom)
  CAF_ADD_ATOM(core_module, caf, leave_atom)
  CAF_ADD_ATOM(core_module, caf, link_atom)
  CAF_ADD_ATOM(core_module, caf, migrate_atom)
  CAF_ADD_ATOM(core_module, caf, monitor_atom)
  CAF_ADD_ATOM(core_module, caf, mul_atom)
  CAF_ADD_ATOM(core_module, caf, ok_atom)
  CAF_ADD_ATOM(core_module, caf, open_atom)
  CAF_ADD_ATOM(core_module, caf, pending_atom)
  CAF_ADD_ATOM(core_module, caf, ping_atom)
  CAF_ADD_ATOM(core_module, caf, pong_atom)
  CAF_ADD_ATOM(core_module, caf, publish_atom)
  CAF_ADD_ATOM(core_module, caf, publish_udp_atom)
  CAF_ADD_ATOM(core_module, caf, put_atom)
  CAF_ADD_ATOM(core_module, caf, receive_atom)
  CAF_ADD_ATOM(core_module, caf, redirect_atom)
  CAF_ADD_ATOM(core_module, caf, registry_lookup_atom)
  CAF_ADD_ATOM(core_module, caf, reset_atom)
  CAF_ADD_ATOM(core_module, caf, resolve_atom)
  CAF_ADD_ATOM(core_module, caf, spawn_atom)
  CAF_ADD_ATOM(core_module, caf, stream_atom)
  CAF_ADD_ATOM(core_module, caf, sub_atom)
  CAF_ADD_ATOM(core_module, caf, subscribe_atom)
  CAF_ADD_ATOM(core_module, caf, sys_atom)
  CAF_ADD_ATOM(core_module, caf, tick_atom)
  CAF_ADD_ATOM(core_module, caf, timeout_atom)
  CAF_ADD_ATOM(core_module, caf, unlink_atom)
  CAF_ADD_ATOM(core_module, caf, unpublish_atom)
  CAF_ADD_ATOM(core_module, caf, unpublish_udp_atom)
  CAF_ADD_ATOM(core_module, caf, unsubscribe_atom)
  CAF_ADD_ATOM(core_module, caf, update_atom)
  CAF_ADD_ATOM(core_module, caf, wait_for_atom)

CAF_END_TYPE_ID_BLOCK(core_module)

namespace caf::detail {

static constexpr type_id_t io_module_begin = id_block::core_module::end;

static constexpr type_id_t io_module_end = io_module_begin + 19;

static constexpr type_id_t net_module_begin = io_module_end;

static constexpr type_id_t net_module_end = net_module_begin + 1;

static_assert(net_module_end <= first_custom_type_id);

} // namespace caf::detail
