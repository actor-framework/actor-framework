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

#ifndef CAF_DETAIL_CAS_WEAK_HPP
#define CAF_DETAIL_CAS_WEAK_HPP

#include <atomic>

#include "caf/config.hpp"

namespace caf {
namespace detail {

template<class T>
bool cas_weak(std::atomic<T>* obj, T* expected, T desired) {
# if (defined(CAF_CLANG) && CAF_COMPILER_VERSION < 30401)                      \
     || (defined(CAF_GCC) && CAF_COMPILER_VERSION < 40803)
  return std::atomic_compare_exchange_strong(obj, expected, desired);
# else
  return std::atomic_compare_exchange_weak(obj, expected, desired);
# endif
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_CAS_WEAK_HPP
