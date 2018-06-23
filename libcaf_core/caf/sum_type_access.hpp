/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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
