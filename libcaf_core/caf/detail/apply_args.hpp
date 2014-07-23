/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_APPLY_ARGS_HPP
#define CAF_DETAIL_APPLY_ARGS_HPP

#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"

namespace caf {
namespace detail {

// this utterly useless function works around a bug in Clang that causes
// the compiler to reject the trailing return type of apply_args because
// "get" is not defined (it's found during ADL)
template<long Pos, class... Ts>
typename tl_at<type_list<Ts...>, Pos>::type get(const type_list<Ts...>&);

template <class F, long... Is, class Tuple>
inline auto apply_args(F& f, detail::int_list<Is...>, Tuple&& tup)
-> decltype(f(get<Is>(tup)...)) {
  return f(get<Is>(tup)...);
}

template <class F, class Tuple, class... Ts>
inline auto apply_args_prefixed(F& f, detail::int_list<>, Tuple&, Ts&&... args)
-> decltype(f(std::forward<Ts>(args)...)) {
  return f(std::forward<Ts>(args)...);
}

template <class F, long... Is, class Tuple, class... Ts>
inline auto apply_args_prefixed(F& f, detail::int_list<Is...>, Tuple& tup,
                Ts&&... args)
-> decltype(f(std::forward<Ts>(args)..., get<Is>(tup)...)) {
  return f(std::forward<Ts>(args)..., get<Is>(tup)...);
}

template <class F, long... Is, class Tuple, class... Ts>
inline auto apply_args_suffxied(F& f, detail::int_list<Is...>, Tuple& tup,
                Ts&&... args)
-> decltype(f(get<Is>(tup)..., std::forward<Ts>(args)...)) {
  return f(get<Is>(tup)..., std::forward<Ts>(args)...);
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_APPLY_ARGS_HPP
