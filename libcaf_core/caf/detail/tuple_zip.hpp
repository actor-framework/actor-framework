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

#ifndef CAF_DETAIL_TUPLE_ZIP_HPP
#define CAF_DETAIL_TUPLE_ZIP_HPP

#include <tuple>

#include "caf/detail/int_list.hpp"

namespace caf {
namespace detail {

template <class F, long... Is, class Tup0, class Tup1>
auto tuple_zip(F& f, detail::int_list<Is...>, Tup0&& tup0, Tup1&& tup1)
-> decltype(std::make_tuple(f(get<Is>(tup0), get<Is>(tup1))...)) {
  return std::make_tuple(f(get<Is>(tup0), get<Is>(tup1))...);
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TUPLE_ZIP_HPP
