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

#include "caf/detail/padded_size.hpp"

namespace caf::detail {

template <size_t Remaining, class T, class... Ts>
struct offset_at_helper {
  static constexpr size_t value = offset_at_helper<Remaining - 1, Ts...>::value
                                  + padded_size_v<T>;
};

template <class T, class... Ts>
struct offset_at_helper<0, T, Ts...> {
  static constexpr size_t value = 0;
};

template <size_t Index, class... Ts>
constexpr size_t offset_at = offset_at_helper<Index, Ts...>::value;

} // namespace caf::detail
