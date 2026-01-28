// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/fwd.hpp"

#include <type_traits>

namespace caf::mixin {

// TODO: legacy API. Deprecate with 0.18, remove with 0.19.
template <class T>
struct is_blocking_requester : std::false_type {};

/// Convenience alias for `is_blocking_requester<T>::value`.
template <class T>
inline constexpr bool is_blocking_requester_v = is_blocking_requester<T>::value;

} // namespace caf::mixin

namespace caf {

// Note: having marker types for blocking and non-blocking may seem redundant,
// because an actor is either the one or the other. However, we cannot conclude
// that an actor is non-blocking if it does not have the blocking marker. Actor
// types such as `local_actor` have neither markers, because they are
// "incomplete", i.e., they serve as base type for both blocking and
// non-blocking actors. Hence, we need both markers even though they are
// mutually exclusive. The same reasoning applies to the dynamically vs.
// statically typed markers.

/// Marker type for dynamically typed actors.
struct dynamically_typed_actor_base {};

/// Marker type for statically typed actors.
struct statically_typed_actor_base {};

/// Marker type for blocking actors.
struct blocking_actor_base {};

/// Marker type for non-blocking actors.
struct non_blocking_actor_base {};

/// Default implementation of `actor_traits` for non-actors (SFINAE-friendly).
/// @relates actor_traits
template <class T, bool ExtendsAbstractActor>
struct default_actor_traits {
  static constexpr bool is_dynamically_typed = false;

  static constexpr bool is_statically_typed = false;

  static constexpr bool is_blocking = false;

  static constexpr bool is_non_blocking = false;

  static constexpr bool is_incomplete = true;
};

/// Default implementation of `actor_traits` for regular actors.
/// @relates actor_traits
template <class T>
struct default_actor_traits<T, true> {
  /// Denotes whether `T` is dynamically typed.
  static constexpr bool is_dynamically_typed
    = std::is_base_of_v<dynamically_typed_actor_base, T>;

  /// Denotes whether `T` is statically typed.
  static constexpr bool is_statically_typed
    = std::is_base_of_v<statically_typed_actor_base, T>;

  /// Denotes whether `T` is a blocking actor type.
  static constexpr bool is_blocking = std::is_base_of_v<blocking_actor_base, T>
                                      || mixin::is_blocking_requester_v<T>;

  /// Denotes whether `T` is a non-blocking actor type.
  static constexpr bool is_non_blocking
    = std::is_base_of_v<non_blocking_actor_base, T>;

  /// Denotes whether `T` is an incomplete actor type that misses one or more
  /// markers.
  static constexpr bool is_incomplete = (!is_dynamically_typed
                                         && !is_statically_typed)
                                        || (!is_blocking && !is_non_blocking);

  static_assert(!is_dynamically_typed || !is_statically_typed,
                "an actor cannot be both statically and dynamically typed");

  static_assert(!is_blocking || !is_non_blocking,
                "an actor cannot be both blocking and non-blocking");
};

/// Provides uniform access to properties of actor types.
template <class T>
struct actor_traits
  : default_actor_traits<T, std::is_base_of_v<abstract_actor, T>> {};

} // namespace caf
