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
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_MAKE_COUNTED_HPP
#define CAF_DETAIL_MAKE_COUNTED_HPP

#include "caf/intrusive_ptr.hpp"

#include "caf/ref_counted.hpp"

#include "caf/mixin/memory_cached.hpp"

#include "caf/detail/memory.hpp"

namespace caf {
namespace detail {

template <class T, class... Ts>
typename std::enable_if<mixin::is_memory_cached<T>::value,
            intrusive_ptr<T>>::type
make_counted(Ts&&... args) {
  return {detail::memory::create<T>(std::forward<Ts>(args)...)};
}

template <class T, class... Ts>
typename std::enable_if<!mixin::is_memory_cached<T>::value,
            intrusive_ptr<T>>::type
make_counted(Ts&&... args) {
  return {new T(std::forward<Ts>(args)...)};
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_MAKE_COUNTED_HPP
