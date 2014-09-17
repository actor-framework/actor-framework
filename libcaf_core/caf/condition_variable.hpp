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

#ifndef CAF_CONDITION_VARIABLE_HPP
#define CAF_CONDITION_VARIABLE_HPP

#ifdef __RIOTBUILD_FLAG


#include <chrono>

#include "mutex.hpp"

namespace caf {

enum class cv_status { no_timeout, timeout };

class condition_variable {

 public:
  using native_handle_type = priority_queue_t*;

  // constexpr condition_variable() : m_queue{NULL} { }
  inline condition_variable() {
    m_queue.first = NULL;
  }
  ~condition_variable();

  void notify_one() noexcept;
  void notify_all() noexcept;

  void wait(unique_lock<mutex>& lock) noexcept;
  template <class Predicate>
  void wait(unique_lock<mutex>& lock, Predicate pred);
  template <class Clock, class Duration>
  cv_status wait_until(unique_lock<mutex>& lock,
                 const std::chrono::time_point<Clock, Duration>& timeout_time);
  template <class Clock, class Duration, class Predicate>
  bool wait_until(unique_lock<mutex>& lock,
                  const std::chrono::time_point<Clock,Duration>& timeout_time,
                  Predicate pred);
  template <class Rep, class Period>
  cv_status wait_for(unique_lock<mutex>& lock,
                     const std::chrono::duration<Rep, Period>& rel_time);
  template <class Rep, class Period, class Predicate>
  bool wait_for(unique_lock<mutex>& lock,
                const std::chrono::duration<Rep, Period>& rel_time,
                Predicate pred);
  inline native_handle_type native_handle() { return &m_queue; }

 private:
  condition_variable(const condition_variable&);
  condition_variable& operator=(const condition_variable&);

  void do_timed_wait(unique_lock<mutex>& lock,
                     std::chrono::time_point<std::chrono::system_clock,
                                             std::chrono::nanoseconds>) noexcept;

  priority_queue_t m_queue;
};

template <class T, class Rep, class Period>
inline typename std::enable_if<std::chrono::__is_duration<T>::value,T>::type
ceil(std::chrono::duration<Rep, Period> duration) {
  using namespace std::chrono;
  T res = duration_cast<T>(duration);
  if (res < duration) {
    ++res;
  }
  return res;
}

template <class Predicate>
void condition_variable::wait(unique_lock<mutex>& lock, Predicate pred) {
  while(!pred()) {
    wait(lock);
  }
}

template <class Clock, class Duration>
cv_status condition_variable::wait_until(unique_lock<mutex>& lock,
                                         const std::chrono::time_point<Clock,
                                                      Duration>& timeout_time) {
    wait_for(lock, timeout_time - Clock::now());
    return Clock::now() < timeout_time ?
      cv_status::no_timeout : cv_status::timeout;
}

template <class Clock, class Duration, class Predicate>
bool condition_variable::wait_until(unique_lock<mutex>& lock,
                    const std::chrono::time_point<Clock,Duration>& timeout_time,
                    Predicate pred) {
  while (!pred()) {
    if (wait_until(lock, timeout_time) == cv_status::timeout) {
      return pred();
    }
  }
  return true;
}

template <class Rep, class Period>
cv_status condition_variable::wait_for(unique_lock<mutex>& lock,
                   const std::chrono::duration<Rep, Period>& timeout_duration) {
    using namespace std::chrono;
    using std::chrono::duration;
    if (timeout_duration <= timeout_duration.zero()) {
      return cv_status::timeout;
    }
    typedef time_point<system_clock, duration<long double, std::nano>> __sys_tpf;
    typedef time_point<system_clock, nanoseconds> __sys_tpi;
    __sys_tpf _Max = __sys_tpi::max();
    system_clock::time_point __s_now = system_clock::now();
    steady_clock::time_point __c_now = steady_clock::now();
    if (_Max - timeout_duration > __s_now) {
      do_timed_wait(lock, __s_now + ceil<nanoseconds>(timeout_duration));
    } else {
      do_timed_wait(lock, __sys_tpi::max());
    }
    return steady_clock::now() - __c_now < timeout_duration ?
      cv_status::no_timeout : cv_status::timeout;
}

template <class Rep, class Period, class Predicate>
inline bool condition_variable::wait_for(unique_lock<mutex>& lock,
                     const std::chrono::duration<Rep, Period>& timeout_duration,
                     Predicate pred) {
    return wait_until(lock, std::chrono::steady_clock::now() + timeout_duration,
                      std::move(pred));
}

} // namespace caf


#else

#include <condition_variable>

using cv_status = std::cv_status;
using condition_variable = std::condition_variable;

#endif


#endif // CAF_CONDITION_VARIABLE_HPP
