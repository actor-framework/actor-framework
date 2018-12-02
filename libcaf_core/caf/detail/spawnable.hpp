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

#include "caf/detail/type_traits.hpp"
#include "caf/infer_handle.hpp"

namespace caf {
namespace detail {

/// Returns whether the function object `F` is spawnable from the actor
/// implementation `Impl` with arguments of type `Ts...`.
template <class F, class Impl, class... Ts>
constexpr bool spawnable() {
  return is_callable_with<F, Ts...>::value
         || is_callable_with<F, Impl*, Ts...>::value;
}

} // namespace detail
} // namespace caf
