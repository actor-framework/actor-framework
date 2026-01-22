// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/scheduler.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace caf::internal {

/// A trivial scheduler implementation that runs on a single dedicated thread.
/// This scheduler is used by detached actors that opt out of cooperative
/// scheduling. Each detached scheduler is pinned to a specific resumable
/// (the actor) and forwards any other resumables to the parent scheduler.
class CAF_CORE_EXPORT detached_scheduler : public scheduler {
public:
  /// Constructs a detached scheduler for the given resumable.
  /// @param pinned The resumable that will run on this scheduler.
  /// @param parent The parent scheduler for forwarding non-pinned resumables.
  detached_scheduler(resumable* pinned, scheduler* parent);

  void schedule(resumable* ptr, uint64_t event_id) override;

  void delay(resumable* ptr, uint64_t event_id) override;

  bool is_system_scheduler() const noexcept override;

  void init(std::thread hdl);

  void run();

  void start() override;

  void stop() override;

private:
  resumable* await();

  /// The resumable that is pinned to this scheduler.
  resumable* pinned_;

  /// The parent scheduler for forwarding non-pinned resumables.
  scheduler* parent_;

  std::thread thread_;
  std::mutex mtx_;
  std::condition_variable cv_;
  resumable* job_ = nullptr;
  bool shutdown_ = false;
};

} // namespace caf::internal
