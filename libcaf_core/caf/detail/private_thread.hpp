// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

class private_thread {
public:
  enum worker_state { active, shutdown_requested, await_resume_or_shutdown };

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
  std::atomic<bool> self_destroyed_;
  std::atomic<scheduled_actor*> self_;
  std::atomic<worker_state> state_;
  actor_system& system_;
};

} // namespace caf::detail
