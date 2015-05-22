/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 * Raphael Hiesgen <raphael.hiesgen (at) haw-hamburg.de>                      *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_CHRONO_HPP
#define CAF_CHRONO_HPP

#include <cstdio>
#include <algorithm>

#ifdef __RIOTBUILD_FLAG

#include "riot/chrono.hpp"

namespace caf {

using time_point = riot::time_point;
inline auto now() -> decltype(time_point()) {
  return riot::now();
}


template <class Duration>
inline time_point& operator+=(time_point& tp, const Duration& d) {
  switch (static_cast<uint32_t>(d.unit)) {
    case 1:
      tp += std::chrono::seconds(d.count);
      break;
    case 1000:
      tp += std::chrono::microseconds(d.count);
      break;
    case 1000000:
      tp += std::chrono::microseconds(1000 * d.count);
      break;
    default:
      break;
  }
  return tp;
}

} // namespace caf

#else

#include <chrono>

namespace caf {

using time_point = std::chrono::high_resolution_clock::time_point;
inline auto now() -> decltype(time_point()) {
  return std::chrono::high_resolution_clock::now();
}

} // namespace caf

#endif

#endif // CAF_CHRONO_HPP
