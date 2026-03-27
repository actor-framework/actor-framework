// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/private_thread_pool.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/resumable.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace caf::detail {

class CAF_CORE_EXPORT private_thread : public private_thread_pool::node {
public:
  void resume(resumable_ptr job);

  bool stop() override;

  static private_thread* launch(actor_system* sys);

  auto id() const {
    return thread_.get_id();
  }

private:
  void run(actor_system* sys);

  std::pair<resumable_ptr, bool> await();

  std::thread thread_;
  std::mutex mtx_;
  std::condition_variable cv_;
  resumable_ptr job_;
  bool shutdown_ = false;
};

} // namespace caf::detail
