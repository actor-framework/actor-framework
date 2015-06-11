/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_LEFT_OR_RIGHT_HPP
#define CAF_LEFT_OR_RIGHT_HPP

#include "caf/unit.hpp"

namespace caf {
namespace detail {

/// Evaluates to `Right` if `Left` == unit_t, `Left` otherwise.
template <class Left, typename Right>
struct left_or_right {
  using type = Left;
};

template <class Right>
struct left_or_right<unit_t, Right> {
  using type = Right;
};

template <class Right>
struct left_or_right<unit_t&, Right> {
  using type = Right;
};

template <class Right>
struct left_or_right<const unit_t&, Right> {
  using type = Right;
};

/// Evaluates to `Right` if `Left` != unit_t, `unit_t` otherwise.
template <class Left, typename Right>
struct if_not_left {
  using type = unit_t;
};

template <class Right>
struct if_not_left<unit_t, Right> {
  using type = Right;
};

} // namespace detail
} // namespace caf

#endif // CAF_LEFT_OR_RIGHT_HPP
