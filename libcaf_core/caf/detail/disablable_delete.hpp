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

#ifndef CAF_DETAIL_DISABLABLE_DELETE_HPP
#define CAF_DETAIL_DISABLABLE_DELETE_HPP

namespace caf {
namespace detail {

class disablable_delete {

 public:

  constexpr disablable_delete() : m_enabled(true) {}

  inline void disable() { m_enabled = false; }

  inline void enable() { m_enabled = true; }

  template <class T>
  inline void operator()(T* ptr) {
    if (m_enabled) delete ptr;
  }

 private:

  bool m_enabled;

};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_DISABLABLE_DELETE_HPP
