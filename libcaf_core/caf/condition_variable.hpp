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

#ifndef CAF_CONDITION_VARIABLE_HPP
#define CAF_CONDITION_VARIABLE_HPP

#ifdef __RIOTBUILD_FLAG


#include "sched.h"
#include "vtimer.h"

#include "caf/mutex.hpp"

#include "caf/chrono.hpp"


namespace caf {

enum class cv_status { no_timeout, timeout };

class condition_variable {

 public:
  using native_handle_type = priority_queue_t*;

  inline condition_variable() {
    m_queue.first = NULL;
  }
  ~condition_variable();

  void notify_one() noexcept;
  void notify_all() noexcept;

  void wait(unique_lock<mutex>& lock) noexcept;
  template <class Predicate>
  void wait(unique_lock<mutex>& lock, Predicate pred);
  cv_status wait_until(unique_lock<mutex>& lock,
                       const time_point& timeout_time);
  template <class Predicate>
  bool wait_until(unique_lock<mutex>& lock, const time_point& timeout_time,
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

template <class Predicate>
bool condition_variable::wait_until(unique_lock<mutex>& lock,
                                    const time_point& timeout_time,
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
  timex_t timeout, before, after;
  auto s = duration_cast<seconds>(timeout_duration);
  timeout.seconds = s.count();
  timeout.microseconds = (duration_cast<microseconds>(timeout_duration - s)).count();
  vtimer_now(&before);
  vtimer_t timer;
  vtimer_set_wakeup(&timer, timeout, sched_active_pid);
  wait(lock);
  vtimer_now(&after);
  vtimer_remove(&timer);
  auto passed = timex_sub(after,before);
  auto cmp = timex_cmp(passed, timeout);
  return cmp < 1  ? cv_status::no_timeout : cv_status::timeout;
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
