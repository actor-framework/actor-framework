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

#ifndef CAF_ACTOR_CAST_HPP
#define CAF_ACTOR_CAST_HPP

#include <type_traits>

namespace caf {

template <class T>
struct actor_cast_access {
  constexpr actor_cast_access() {
    // nop
  }

  template<class U>
  T operator()(U& y) const {
    return {y.get(), true};
  }

  template<class U>
  typename std::enable_if<std::is_rvalue_reference<U&&>::value, T>::type
  operator()(U&& y) const {
    return {y.release(), false};
  }
};

template <class T>
struct actor_cast_access<T*> {
  constexpr actor_cast_access() {
    // nop
  }

  template<class U>
  T* operator()(const U& y) const {
    return y.get();
  }
};

/// Converts actor handle `what` to a different actor
/// handle or raw pointer of type `T`.
template <class T, class U>
T actor_cast(U&& what) {
  actor_cast_access<T> f;
  return f(std::forward<U>(what));
}

} // namespace caf

#endif // CAF_ACTOR_CAST_HPP
