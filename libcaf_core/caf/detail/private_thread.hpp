// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/private_thread_pool.hpp"
#include "caf/fwd.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace caf::detail {

class CAF_CORE_EXPORT private_thread : public private_thread_pool::node {
public:
  void resume(resumable* ptr);

  bool stop() override;

  static private_thread* launch(actor_system* sys);

private:
  void run(actor_system* sys);

  std::pair<resumable*, bool> await();

  std::thread thread_;
  std::mutex mtx_;
  std::condition_variable cv_;
  resumable* job_ = nullptr;
  bool shutdown_ = false;
};

} // namespace caf::detail
