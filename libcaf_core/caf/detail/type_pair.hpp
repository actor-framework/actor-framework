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

namespace caf {
namespace detail {

template <class First, typename Second>
struct type_pair {
  using first = First;
  using second = Second;
};

template <class First, typename Second>
struct to_type_pair {
  using type = type_pair<First, Second>;
};

template <class What>
struct is_type_pair {
  static constexpr bool value = false;
};

template <class First, typename Second>
struct is_type_pair<type_pair<First, Second>> {
  static constexpr bool value = true;
};

} // namespace detail
} // namespace caf

