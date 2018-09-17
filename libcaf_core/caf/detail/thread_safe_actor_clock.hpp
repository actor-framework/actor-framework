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

#pragma once

#include <mutex>
#include <atomic>
#include <condition_variable>

#include "caf/detail/simple_actor_clock.hpp"

namespace caf {
namespace detail {

class thread_safe_actor_clock : public simple_actor_clock {
public:
  using super = simple_actor_clock;

  thread_safe_actor_clock();

  void set_ordinary_timeout(time_point t, abstract_actor* self,
                           atom_value type, uint64_t id) override;

  void set_request_timeout(time_point t, abstract_actor* self,
                           message_id id) override;

  void cancel_ordinary_timeout(abstract_actor* self, atom_value type) override;

  void cancel_request_timeout(abstract_actor* self, message_id id) override;

  void cancel_timeouts(abstract_actor* self) override;

  void schedule_message(time_point t, strong_actor_ptr receiver,
                        mailbox_element_ptr content) override;

  void schedule_message(time_point t, group target, strong_actor_ptr sender,
                        message content) override;

  void cancel_all() override;

  void run_dispatch_loop();

  void cancel_dispatch_loop();

private:
  std::recursive_mutex mx_;
  std::condition_variable_any cv_;
  std::atomic<bool> done_;
};

} // namespace detail
} // namespace caf

