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

#include <chrono>
#include <cstdio>
#include <algorithm>

#ifdef __RIOTBUILD_FLAG

#include "time.h"
#include "vtimer.h"

namespace caf {

class duration;

namespace {
  constexpr uint32_t microsec_in_secs = 1000000;
} // namespace anaonymous

class time_point {
  using native_handle_type = timex_t;

 public:
  inline time_point() : m_handle{0,0} { }
  inline time_point(timex_t&& tp) : m_handle(tp) { }
  constexpr time_point(const time_point& tp) = default;
  constexpr time_point(time_point&& tp) = default;

  inline native_handle_type native_handle() const { return m_handle; }

  time_point& operator+=(const caf::duration& d);
  template<class Rep, class Period>
  inline time_point& operator+=(const std::chrono::duration<Rep,Period>& d) {
    auto s = std::chrono::duration_cast<std::chrono::seconds>(d);
    auto m = (std::chrono::duration_cast<std::chrono::microseconds>(d) - s);
    m_handle.seconds += s.count();
    m_handle.microseconds += m.count();
    adjust_overhead();
    return *this;
  }

  inline uint32_t seconds() const { return m_handle.seconds; }
  inline uint32_t microseconds() const { return m_handle.microseconds; }

 private:
  timex_t m_handle;
  void inline adjust_overhead() {
    while (m_handle.microseconds >= microsec_in_secs) {
      m_handle.seconds += 1;
      m_handle.microseconds -= microsec_in_secs;
    }
  }
};

inline time_point now() {
  timex_t tp;
  vtimer_now(&tp);
  return time_point(std::move(tp));
}

inline bool operator<(const time_point& lhs, const time_point& rhs) {
    return lhs.seconds() < rhs.seconds() ||
           (lhs.seconds() == rhs.seconds() &&
            lhs.microseconds() < rhs.microseconds());
}

inline bool operator>(const time_point& lhs, const time_point& rhs) {
    return rhs < lhs;
}

inline bool operator<=(const time_point& lhs, const time_point& rhs) {
    return !(rhs < lhs);
}

inline bool operator>=(const time_point& lhs, const time_point& rhs) {
    return !(lhs < rhs);
}

} // namespace caf

#else

namespace caf {

using time_point = std::chrono::high_resolution_clock::time_point;
inline std::chrono::time_point<std::chrono::high_resolution_clock> now() {
  return std::chrono::high_resolution_clock::now();
}

// todo mapping for caf

} // namespace caf

#endif

#endif // CAF_CHRONO_HPP
