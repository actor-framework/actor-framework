/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_DETAIL_PRIVATE_THREAD_HPP
#define CAF_DETAIL_PRIVATE_THREAD_HPP

#include <mutex>
#include <condition_variable>

#include "caf/fwd.hpp"

namespace caf {
namespace detail {

class private_thread {
public:
  enum worker_state {
    active,
    shutdown_requested,
    await_resume_or_shutdown
  };

  explicit private_thread(scheduled_actor* self);

  void run();

  bool await_resume();

  void resume();

  void shutdown();

  static void exec(private_thread* this_ptr);

  void notify_self_destroyed();

  void await_self_destroyed();

  void start();

private:
  std::mutex mtx_;
  std::condition_variable cv_;
  volatile bool self_destroyed_;
  volatile scheduled_actor* self_;
  volatile worker_state state_;
  actor_system& system_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_PRIVATE_THREAD_HPP
