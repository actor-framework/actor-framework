/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include <type_traits>

#include "caf/fwd.hpp"

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
  static constexpr bool is_dynamically_typed = //
    std::is_base_of<dynamically_typed_actor_base, T>::value;

  /// Denotes whether `T` is statically typed.
  static constexpr bool is_statically_typed = //
    std::is_base_of<statically_typed_actor_base, T>::value;

  /// Denotes whether `T` is a blocking actor type.
  static constexpr bool is_blocking = //
    std::is_base_of<blocking_actor_base, T>::value;

  /// Denotes whether `T` is a non-blocking actor type.
  static constexpr bool is_non_blocking = //
    std::is_base_of<non_blocking_actor_base, T>::value;

  /// Denotes whether `T` is an incomplete actor type that misses one or more
  /// markers.
  static constexpr bool is_incomplete = //
    (!is_dynamically_typed && !is_statically_typed)
    || (!is_blocking && !is_non_blocking);

  static_assert(!is_dynamically_typed || !is_statically_typed,
                "an actor cannot be both statically and dynamically typed");

  static_assert(!is_blocking || !is_non_blocking,
                "an actor cannot be both blocking and non-blocking");
};

/// Provides uniform access to properties of actor types.
template <class T>
struct actor_traits
  : default_actor_traits<T, std::is_base_of<abstract_actor, T>::value> {};

} // namespace caf
