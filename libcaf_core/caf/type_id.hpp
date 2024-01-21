// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/fwd.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/is_complete.hpp"
#include "caf/detail/pp.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/fwd.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>

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
constexpr std::string_view type_name_by_id_v = type_name_by_id<I>::value;

/// Convenience type that resolves to `type_name_by_id<type_id_v<T>>`.
template <class T>
struct type_name;

/// Convenience specialization that enables generic code to not treat `void`
/// manually.
template <>
struct type_name<void> {
  static constexpr std::string_view value = "void";
};

/// Convenience alias for `type_name<T>::value`.
/// @relates type_name
template <class T>
constexpr std::string_view type_name_v = type_name<T>::value;

/// The first type ID not reserved by CAF and its modules.
constexpr type_id_t first_custom_type_id = 200;

/// Checks whether `type_id` is specialized for `T`.
template <class T>
constexpr bool has_type_id_v = detail::is_complete<type_id<T>>;

/// Returns `type_name_v<T>` if available, "anonymous" otherwise.
template <class T>
std::string_view type_name_or_anonymous() {
  if constexpr (detail::is_complete<type_name<T>>)
    return type_name<T>::value;
  else
    return "anonymous";
}

/// Returns `type_id_v<T>` if available, `invalid_type_id` otherwise.
template <class T>
type_id_t type_id_or_invalid() {
  if constexpr (detail::is_complete<type_id<T>>)
    return type_id<T>::value;
  else
    return invalid_type_id;
}

/// Returns the type name for @p type or an empty string if @p type is an
/// invalid ID.
CAF_CORE_EXPORT std::string_view query_type_name(type_id_t type);

/// Returns the type for @p name or `invalid_type_id` if @p name is unknown.
CAF_CORE_EXPORT type_id_t query_type_id(std::string_view name);

/// Translates between human-readable type names and type IDs.
class CAF_CORE_EXPORT type_id_mapper {
public:
  virtual ~type_id_mapper();

  /// Returns the type name for @p type or an empty string if @p type is an
  /// invalid ID.
  virtual std::string_view operator()(type_id_t type) const = 0;

  /// Returns the type for @p name or `invalid_type_id` if @p name is unknown.
  virtual type_id_t operator()(std::string_view name) const = 0;
};

/// Dispatches to @ref query_type_name and @ref query_type_id.
class CAF_CORE_EXPORT default_type_id_mapper : public type_id_mapper {
public:
  std::string_view operator()(type_id_t type) const override;

  type_id_t operator()(std::string_view name) const override;
};

} // namespace caf

// -- CAF_BEGIN_TYPE_ID_BLOCK --------------------------------------------------

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

// -- CAF_ADD_TYPE_ID ----------------------------------------------------------

#ifdef CAF_MSVC
#  define CAF_DETAIL_NEXT_TYPE_ID(project_name, fully_qualified_name)          \
    template <>                                                                \
    struct type_id<::CAF_PP_EXPAND fully_qualified_name> {                     \
      static constexpr type_id_t value                                         \
        = id_block::project_name##_first_type_id                               \
          + (CAF_PP_CAT(CAF_PP_COUNTER, ())                                    \
             - id_block::project_name##_type_id_counter_init - 1);             \
    };
#else
#  define CAF_DETAIL_NEXT_TYPE_ID(project_name, fully_qualified_name)          \
    template <>                                                                \
    struct type_id<::CAF_PP_EXPAND fully_qualified_name> {                     \
      static constexpr type_id_t value                                         \
        = id_block::project_name##_first_type_id                               \
          + (__COUNTER__ - id_block::project_name##_type_id_counter_init - 1); \
    };
#endif

#define CAF_ADD_TYPE_ID_2(project_name, fully_qualified_name)                  \
  namespace caf {                                                              \
  CAF_DETAIL_NEXT_TYPE_ID(project_name, fully_qualified_name)                  \
  template <>                                                                  \
  struct type_by_id<type_id<::CAF_PP_EXPAND fully_qualified_name>::value> {    \
    using type = ::CAF_PP_EXPAND fully_qualified_name;                         \
  };                                                                           \
  template <>                                                                  \
  struct type_name<::CAF_PP_EXPAND fully_qualified_name> {                     \
    static constexpr std::string_view value                                    \
      = CAF_PP_STR(CAF_PP_EXPAND fully_qualified_name);                        \
  };                                                                           \
  template <>                                                                  \
  struct type_name_by_id<type_id<::CAF_PP_EXPAND fully_qualified_name>::value> \
    : type_name<::CAF_PP_EXPAND fully_qualified_name> {};                      \
  }

#define CAF_ADD_TYPE_ID_3(project_name, fully_qualified_name, user_type_name)  \
  namespace caf {                                                              \
  CAF_DETAIL_NEXT_TYPE_ID(project_name, fully_qualified_name)                  \
  template <>                                                                  \
  struct type_by_id<type_id<::CAF_PP_EXPAND fully_qualified_name>::value> {    \
    using type = ::CAF_PP_EXPAND fully_qualified_name;                         \
  };                                                                           \
  template <>                                                                  \
  struct type_name<::CAF_PP_EXPAND fully_qualified_name> {                     \
    static constexpr std::string_view value = user_type_name;                  \
  };                                                                           \
  template <>                                                                  \
  struct type_name_by_id<type_id<::CAF_PP_EXPAND fully_qualified_name>::value> \
    : type_name<::CAF_PP_EXPAND fully_qualified_name> {};                      \
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

// -- CAF_ADD_TYPE_ID_FROM_EXPR ------------------------------------------------

#ifdef CAF_MSVC
#  define CAF_DETAIL_NEXT_TYPE_ID_FROM_EXPR(project_name, type_expr)           \
    template <>                                                                \
    struct type_id<CAF_PP_EXPAND type_expr> {                                  \
      static constexpr type_id_t value                                         \
        = id_block::project_name##_first_type_id                               \
          + (CAF_PP_CAT(CAF_PP_COUNTER, ())                                    \
             - id_block::project_name##_type_id_counter_init - 1);             \
    };
#else
#  define CAF_DETAIL_NEXT_TYPE_ID_FROM_EXPR(project_name, type_expr)           \
    template <>                                                                \
    struct type_id<CAF_PP_EXPAND type_expr> {                                  \
      static constexpr type_id_t value                                         \
        = id_block::project_name##_first_type_id                               \
          + (__COUNTER__ - id_block::project_name##_type_id_counter_init - 1); \
    };
#endif

#define CAF_ADD_TYPE_ID_FROM_EXPR_2(project_name, type_expr)                   \
  namespace caf {                                                              \
  CAF_DETAIL_NEXT_TYPE_ID_FROM_EXPR(project_name, type_expr)                   \
  template <>                                                                  \
  struct type_by_id<type_id<CAF_PP_EXPAND type_expr>::value> {                 \
    using type = CAF_PP_EXPAND type_expr;                                      \
  };                                                                           \
  template <>                                                                  \
  struct type_name<CAF_PP_EXPAND type_expr> {                                  \
    static constexpr std::string_view value                                    \
      = CAF_PP_STR(CAF_PP_EXPAND type_expr);                                   \
  };                                                                           \
  template <>                                                                  \
  struct type_name_by_id<type_id<CAF_PP_EXPAND type_expr>::value>              \
    : type_name<CAF_PP_EXPAND type_expr> {};                                   \
  }

#define CAF_ADD_TYPE_ID_FROM_EXPR_3(project_name, type_expr, user_type_name)   \
  namespace caf {                                                              \
  CAF_DETAIL_NEXT_TYPE_ID_FROM_EXPR(project_name, type_expr)                   \
  template <>                                                                  \
  struct type_by_id<type_id<CAF_PP_EXPAND type_expr>::value> {                 \
    using type = CAF_PP_EXPAND type_expr;                                      \
  };                                                                           \
  template <>                                                                  \
  struct type_name<CAF_PP_EXPAND type_expr> {                                  \
    static constexpr std::string_view value = user_type_name;                  \
  };                                                                           \
  template <>                                                                  \
  struct type_name_by_id<type_id<CAF_PP_EXPAND type_expr>::value>              \
    : type_name<CAF_PP_EXPAND type_expr> {};                                   \
  }

/// @def CAF_ADD_TYPE_ID_FROM_EXPR(project_name, type_expr, user_type_name)
/// Assigns the next free type ID to the type resulting from `type_expr`.
/// @param project_name User-defined name for type ID block.
/// @param type_expr A compile-time expression resulting in a type, e.g., a
///                  `decltype` statement.
/// @param user_type_name Optional parameter. If present, defines the content of
///                       `caf::type_name`. Defaults to `type_expr`.
#ifdef CAF_MSVC
#  define CAF_ADD_TYPE_ID_FROM_EXPR(...)                                       \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_ADD_TYPE_ID_FROM_EXPR_,                     \
                               __VA_ARGS__)(__VA_ARGS__),                      \
               CAF_PP_EMPTY())
#else
#  define CAF_ADD_TYPE_ID_FROM_EXPR(...)                                       \
    CAF_PP_OVERLOAD(CAF_ADD_TYPE_ID_FROM_EXPR_, __VA_ARGS__)(__VA_ARGS__)
#endif

// -- CAF_ADD_ATOM -------------------------------------------------------------

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

// -- CAF_END_TYPE_ID_BLOCK ----------------------------------------------------

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

// -- type ID block for the core module ----------------------------------------

CAF_BEGIN_TYPE_ID_BLOCK(core_module, 0)

  // -- C types ----------------------------------------------------------------

  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (bool) )
  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (double) )
  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (float) )
  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (int16_t))
  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (int32_t))
  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (int64_t))
  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (int8_t))
  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (long double), "ldouble")
  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (uint16_t))
  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (uint32_t))
  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (uint64_t))
  CAF_ADD_TYPE_ID_FROM_EXPR(core_module, (uint8_t))

  // -- STL types --------------------------------------------------------------

  CAF_ADD_TYPE_ID(core_module, (std::string))
  CAF_ADD_TYPE_ID(core_module, (std::u16string))
  CAF_ADD_TYPE_ID(core_module, (std::u32string))
  CAF_ADD_TYPE_ID(core_module, (std::set<std::string>) )

  // -- CAF types --------------------------------------------------------------

  CAF_ADD_TYPE_ID(core_module, (caf::action))
  CAF_ADD_TYPE_ID(core_module, (caf::actor))
  CAF_ADD_TYPE_ID(core_module, (caf::actor_addr))
  CAF_ADD_TYPE_ID(core_module, (caf::async::batch))
  CAF_ADD_TYPE_ID(core_module, (caf::byte_buffer))
  CAF_ADD_TYPE_ID(core_module, (caf::config_value))
  CAF_ADD_TYPE_ID(core_module, (caf::cow_string))
  CAF_ADD_TYPE_ID(core_module, (caf::cow_u16string))
  CAF_ADD_TYPE_ID(core_module, (caf::cow_u32string))
  CAF_ADD_TYPE_ID(core_module, (caf::down_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::error))
  CAF_ADD_TYPE_ID(core_module, (caf::exit_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::exit_reason))
  CAF_ADD_TYPE_ID(core_module, (caf::hashed_node_id))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv4_address))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv4_endpoint))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv4_subnet))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv6_address))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv6_endpoint))
  CAF_ADD_TYPE_ID(core_module, (caf::ipv6_subnet))
  CAF_ADD_TYPE_ID(core_module, (caf::json_array))
  CAF_ADD_TYPE_ID(core_module, (caf::json_object))
  CAF_ADD_TYPE_ID(core_module, (caf::json_value))
  CAF_ADD_TYPE_ID(core_module, (caf::message))
  CAF_ADD_TYPE_ID(core_module, (caf::message_id))
  CAF_ADD_TYPE_ID(core_module, (caf::node_down_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::node_id))
  CAF_ADD_TYPE_ID(core_module, (caf::none_t))
  CAF_ADD_TYPE_ID(core_module, (caf::pec))
  CAF_ADD_TYPE_ID(core_module, (caf::sec))
  CAF_ADD_TYPE_ID(core_module, (caf::settings))
  CAF_ADD_TYPE_ID(core_module, (caf::shared_action_ptr))
  CAF_ADD_TYPE_ID(core_module, (caf::stream))
  CAF_ADD_TYPE_ID(core_module, (caf::stream_abort_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::stream_ack_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::stream_batch_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::stream_cancel_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::stream_close_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::stream_demand_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::stream_open_msg))
  CAF_ADD_TYPE_ID(core_module, (caf::strong_actor_ptr))
  CAF_ADD_TYPE_ID(core_module, (caf::timespan))
  CAF_ADD_TYPE_ID(core_module, (caf::timestamp))
  CAF_ADD_TYPE_ID(core_module, (caf::unit_t))
  CAF_ADD_TYPE_ID(core_module, (caf::uri))
  CAF_ADD_TYPE_ID(core_module, (caf::uuid))
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
