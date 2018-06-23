/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {


/// Moves the value from `x` if it is not a pointer (e.g., `optional` or
/// `expected`), returns `*x` otherwise.
template <class T>
T& move_if_not_ptr(T* x) {
  return *x;
}

/// Moves the value from `x` if it is not a pointer (e.g., `optional` or
/// `expected`), returns `*x` otherwise.
template <class T, class E = enable_if_t<!std::is_pointer<T>::value>>
T&& move_if_not_ptr(T& x) {
  return std::move(*x);
}

} // namespace detail
} // namespace caf


