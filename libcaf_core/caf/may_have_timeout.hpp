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

#ifndef MAY_HAVE_TIMEOUT_HPP
#define MAY_HAVE_TIMEOUT_HPP

namespace caf {

template <class F>
struct timeout_definition;

class behavior;

template <class T>
struct may_have_timeout {
  static constexpr bool value = false;

};

template <>
struct may_have_timeout<behavior> {
  static constexpr bool value = true;

};

template <class F>
struct may_have_timeout<timeout_definition<F>> {
  static constexpr bool value = true;

};

} // namespace caf

#endif // MAY_HAVE_TIMEOUT_HPP
