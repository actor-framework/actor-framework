/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_DETAIL_PSEUDO_TUPLE_HPP
#define CAF_DETAIL_PSEUDO_TUPLE_HPP

#include <cstddef>

#include "caf/param.hpp"
#include "caf/config.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

// tuple-like access to an array of void pointers that is
// also aware of the semantics of param<T>
template <class... Ts>
struct pseudo_tuple {
  using pointer = void*;
  using const_pointer = const void*;

  pointer data[sizeof...(Ts) > 0 ? sizeof...(Ts) : 1];

  bool shared_access;

  template <class Tuple>
  pseudo_tuple(const Tuple& xs) : data(), shared_access(xs.shared()) {
    CAF_ASSERT(sizeof...(Ts) == xs.size());
    for (size_t i = 0; i < xs.size(); ++i)
      data[i] = const_cast<void*>(xs.get(i));
  }

  inline const_pointer at(size_t p) const {
    return data[p];
  }

  inline pointer get_mutable(size_t p) {
    return data[p];
  }

  inline pointer& operator[](size_t p) {
    return data[p];
  }
};

template <class T>
struct pseudo_tuple_access {
  using result_type = T&;

  template <class Tuple>
  static T& get(Tuple& xs, size_t pos) {
    auto vp = xs.get_mutable(pos);
    CAF_ASSERT(vp != nullptr);
    return *reinterpret_cast<T*>(vp);
  }
};

template <class T>
struct pseudo_tuple_access<const T> {
  using result_type = const T&;

  template <class Tuple>
  static const T& get(const Tuple& xs, size_t pos) {
    auto vp = xs.at(pos);
    CAF_ASSERT(vp != nullptr);
    return *reinterpret_cast<const T*>(vp);
  }
};

template <class T>
struct pseudo_tuple_access<param<T>> {
  using result_type = param<T>;

  template <class Tuple>
  static result_type get(const Tuple& xs, size_t pos) {
    auto vp = xs.at(pos);
    CAF_ASSERT(vp != nullptr);
    return {vp, xs.shared_access};
  }
};

template <class T>
struct pseudo_tuple_access<const param<T>> : pseudo_tuple_access<param<T>> {
  // nop
};

template <size_t N, class... Ts>
typename pseudo_tuple_access<
  const typename detail::type_at<N, Ts...>::type
>::result_type
get(const detail::pseudo_tuple<Ts...>& tv) {
  static_assert(N < sizeof...(Ts), "N >= tv.size()");
  using f = pseudo_tuple_access<const typename detail::type_at<N, Ts...>::type>;
  return f::get(tv, N);
}

template <size_t N, class... Ts>
typename pseudo_tuple_access<
  typename detail::type_at<N, Ts...>::type
>::result_type
get(detail::pseudo_tuple<Ts...>& tv) {
  static_assert(N < sizeof...(Ts), "N >= tv.size()");
  using f = pseudo_tuple_access<typename detail::type_at<N, Ts...>::type>;
  return f::get(tv, N);
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_PSEUDO_TUPLE_HPP
