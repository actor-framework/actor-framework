// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"

namespace caf {

/// Tag type to indicate that the system should keep a strong reference to an
/// actor after passing it the `self` pointer.
struct strong_self_ref_t {
  using handle_type = strong_actor_ptr;
};

/// Tag to indicate that the system should keep a strong reference to an actor
/// after passing it the `self` pointer.
inline constexpr strong_self_ref_t strong_self_ref = {};

/// Tag type to indicate that the system should keep a weak reference to an
/// actor after passing it the `self` pointer.
struct weak_self_ref_t {
  using handle_type = weak_actor_ptr;
};

/// Tag to indicate that the system should keep a weak reference to an actor
/// after passing it the `self` pointer.
inline constexpr weak_self_ref_t weak_self_ref = {};

namespace detail {

template <class T>
struct is_self_ref_tag_oracle {
  static constexpr bool value = false;
};

template <>
struct is_self_ref_tag_oracle<strong_self_ref_t> {
  static constexpr bool value = true;
};

template <>
struct is_self_ref_tag_oracle<weak_self_ref_t> {
  static constexpr bool value = true;
};

} // namespace detail

/// Evaluates to `true` if `T` is either `strong_self_ref_t` or
/// `weak_self_ref_t`.
template <class T>
inline constexpr bool is_self_ref_tag
  = detail::is_self_ref_tag_oracle<T>::value;

} // namespace caf
