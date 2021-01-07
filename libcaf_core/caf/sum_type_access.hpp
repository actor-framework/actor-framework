// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

namespace caf {

/// Specializing this trait allows users to enable `holds_alternative`, `get`,
/// `get_if`, and `visit` for any user-defined sum type.
/// @relates SumType
template <class T>
struct sum_type_access {
  static constexpr bool specialized = false;
};

/// Evaluates to `true` if `T` specializes `sum_type_access`.
/// @relates SumType
template <class T>
struct has_sum_type_access {
  static constexpr bool value = sum_type_access<T>::specialized;
};

} // namespace caf
