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

#ifndef CAF_NO_SCHEDULING_HPP
#define CAF_NO_SCHEDULING_HPP

#include <mutex>
#include <thread>
#include <chrono>
#include <limits>
#include <condition_variable>

#include "caf/duration.hpp"
#include "caf/exit_reason.hpp"

#include "caf/policy/scheduling_policy.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/actor_registry.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/detail/single_reader_queue.hpp"


#include "caf/actor_ostream.hpp"



namespace caf {
namespace policy {

class no_scheduling {
  using lock_type = std::unique_lock<std::mutex>;
 public:
  using timeout_type = std::chrono::high_resolution_clock::time_point;

  template <class Actor>
  void enqueue(Actor* self, const actor_addr& sender, message_id mid,
               message& msg, execution_unit*) {
    auto ptr = self->new_mailbox_element(sender, mid, std::move(msg));
    // returns false if mailbox has been closed
    if (!self->mailbox().synchronized_enqueue(m_mtx, m_cv, ptr)) {
      if (mid.is_request()) {
        detail::sync_request_bouncer srb{self->exit_reason()};
        srb(sender, mid);
      }
    }
  }

  template <class Actor>
  void launch(Actor* self, execution_unit*) {
    CAF_REQUIRE(self != nullptr);
    CAF_PUSH_AID(self->id());
    CAF_LOG_TRACE(CAF_ARG(self));
    intrusive_ptr<Actor> mself{self};
    self->attach_to_scheduler();
    std::thread([=] {
      CAF_PUSH_AID(mself->id());
      CAF_LOG_TRACE("");
      auto max_throughput = std::numeric_limits<size_t>::max();
      while (mself->resume(nullptr, max_throughput) != resumable::done) {
        // await new data before resuming actor
        await_data(mself.get());
        CAF_REQUIRE(self->mailbox().blocked() == false);
      }
      self->detach_from_scheduler();
    }).detach();
  }

  // await_data is being called from no_resume (only)
  template <class Actor>
  void await_data(Actor* self) {
    if (self->has_next_message()) return;
    self->mailbox().synchronized_await(m_mtx, m_cv);
  }

  // this additional member function is needed to implement
  // timer_actor (see scheduler.cpp)
  template <class Actor, class TimePoint>
  bool await_data(Actor* self, const TimePoint& tp) {
    if (self->has_next_message()) return true;
    return self->mailbox().synchronized_await(m_mtx, m_cv, tp);
  }

 private:
  std::mutex m_mtx;
  std::condition_variable m_cv;
};

} // namespace policy
} // namespace caf

#endif // CAF_NO_SCHEDULING_HPP
