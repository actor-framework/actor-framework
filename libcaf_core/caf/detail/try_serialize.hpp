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

#ifndef CAF_DETAIL_TRY_SERIALIZE_HPP
#define CAF_DETAIL_TRY_SERIALIZE_HPP

namespace caf {
namespace detail {

template <class T, class U>
auto try_serialize(T& in_or_out, U* x) -> decltype(in_or_out & *x) {
  in_or_out & *x;
}

template <class T>
void try_serialize(T&, const void*) {
  // nop
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TRY_SERIALIZE_HPP
