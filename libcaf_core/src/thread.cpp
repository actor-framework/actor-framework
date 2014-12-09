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

#include "vtimer.h"

#include <cerrno>
#include <system_error>

#include "caf/thread.hpp"

using namespace std;

namespace caf {

thread::~thread() {
  if (joinable()) {
    terminate();
  }
}

void thread::join() {
  if (this->get_id() == this_thread::get_id()) {
    throw system_error(make_error_code(errc::resource_deadlock_would_occur),
                       "Joining this leads to a deadlock.");
  }
  if (joinable()) {
    auto status = thread_getstatus(m_handle);
    if (status != STATUS_NOT_FOUND && status != STATUS_STOPPED) {
      m_data->joining_thread = sched_active_pid;
      thread_sleep();
    }
    m_handle = thread_uninitialized;
  } else {
    throw system_error(make_error_code(errc::invalid_argument),
                       "Can not join an unjoinable thread.");
  }
  // missing: no_such_process system error
}

void thread::detach() {
  if (joinable()) {
    m_handle = thread_uninitialized;
  } else {
    throw system_error(make_error_code(errc::invalid_argument),
                       "Can not detach an unjoinable thread.");
  }
}

unsigned thread::hardware_concurrency() noexcept {
  return 1;
}

namespace this_thread {

void sleep_for(const chrono::nanoseconds& ns) {
  using namespace chrono;
  if (ns > nanoseconds::zero()) {
    seconds s = duration_cast<seconds>(ns);
    microseconds ms = duration_cast<microseconds>(ns);
    timex_t reltime;
    constexpr uint32_t uint32_max = numeric_limits<uint32_t>::max();
    if (s.count() < uint32_max) {
      reltime.seconds      = static_cast<uint32_t>(s.count());
      reltime.microseconds =
        static_cast<uint32_t>(duration_cast<microseconds>((ms-s)).count());
    } else {
      reltime.seconds      = uint32_max;
      reltime.microseconds = uint32_max;
    }
    vtimer_t timer;
    vtimer_set_wakeup(&timer, reltime, sched_active_pid);
    thread_sleep();
    vtimer_remove(&timer);
  }
}

void sleep_until(const time_point& sleep_time) {
  using namespace std::chrono;
  mutex mtx;
  condition_variable cv;
  unique_lock<mutex> lk(mtx);
  while(now() < sleep_time) {
    cv.wait_until(lk,sleep_time);
  }
}

} // namespace this_thread

} // namespace caf
