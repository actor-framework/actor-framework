/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/thread_safe_actor_clock.hpp"

namespace caf {
namespace detail {

namespace {

using guard_type = std::unique_lock<std::mutex>;

} // namespace <anonymous>

thread_safe_actor_clock::thread_safe_actor_clock() : done_(false) {
  // nop
}

void thread_safe_actor_clock::set_ordinary_timeout(time_point t,
                                                  abstract_actor* self,
                                                  atom_value type,
                                                  uint64_t id) {
  guard_type guard{mx_};
  super::set_ordinary_timeout(t, self, type, id);
  cv_.notify_all();
}

void thread_safe_actor_clock::set_request_timeout(time_point t,
                                                  abstract_actor* self,
                                                  message_id id) {
  guard_type guard{mx_};
  super::set_request_timeout(t, self, id);
  cv_.notify_all();
}

void thread_safe_actor_clock::cancel_ordinary_timeout(abstract_actor* self,
                                                      atom_value type) {
  guard_type guard{mx_};
  super::cancel_ordinary_timeout(self, type);
  cv_.notify_all();
}

void thread_safe_actor_clock::cancel_request_timeout(abstract_actor* self,
                                                     message_id id) {
  guard_type guard{mx_};
  super::cancel_request_timeout(self, id);
  cv_.notify_all();
}

void thread_safe_actor_clock::cancel_timeouts(abstract_actor* self) {
  guard_type guard{mx_};
  super::cancel_timeouts(self);
  cv_.notify_all();
}

void thread_safe_actor_clock::schedule_message(time_point t,
                                               strong_actor_ptr receiver,
                                               mailbox_element_ptr content) {
  guard_type guard{mx_};
  super::schedule_message(t, std::move(receiver), std::move(content));
  cv_.notify_all();
}

void thread_safe_actor_clock::schedule_message(time_point t, group target,
                                               strong_actor_ptr sender,
                                               message content) {
  guard_type guard{mx_};
  super::schedule_message(t, std::move(target), std::move(sender),
                          std::move(content));
  cv_.notify_all();
}

void thread_safe_actor_clock::cancel_all() {
  guard_type guard{mx_};
  super::cancel_all();
  cv_.notify_all();
}

void thread_safe_actor_clock::run_dispatch_loop() {
  visitor f{this};
  guard_type guard{mx_};
  while (done_ == false) {
    // Wait for non-empty schedule.
    if (schedule_.empty()) {
      cv_.wait(guard);
    } else {
      auto tout = schedule_.begin()->first;
      cv_.wait_until(guard, tout);
    }
    // Double-check whether schedule is non-empty and execute it.
    if (!schedule_.empty()) {
      auto t = now();
      auto i = schedule_.begin();
      while (i != schedule_.end() && i->first <= t) {
        visit(f, i->second);
        i = schedule_.erase(i);
      }
    }
  }
}

void thread_safe_actor_clock::cancel_dispatch_loop() {
  guard_type guard{mx_};
  done_ = true;
  cv_.notify_all();
}

} // namespace detail
} // namespace caf
