// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/internal/detached_scheduler.hpp"

#include "caf/detail/assert.hpp"
#include "caf/log/core.hpp"
#include "caf/resumable.hpp"

namespace caf::internal {

detached_scheduler::detached_scheduler(resumable* pinned, scheduler* parent)
  : pinned_(pinned), parent_(parent) {
  CAF_ASSERT(pinned != nullptr);
  CAF_ASSERT(parent != nullptr);
}

void detached_scheduler::schedule(resumable* ptr, uint64_t event_id) {
  CAF_ASSERT(ptr != nullptr);
  if (ptr != pinned_) {
    parent_->schedule(ptr, event_id);
    return;
  }
  std::lock_guard guard{mtx_};
  CAF_ASSERT(job_ == nullptr);
  job_ = ptr;
  cv_.notify_all();
}

void detached_scheduler::delay(resumable* ptr, uint64_t event_id) {
  CAF_ASSERT(ptr != nullptr);
  if (ptr != pinned_) {
    parent_->delay(ptr, event_id);
    return;
  }
  std::lock_guard guard{mtx_};
  CAF_ASSERT(job_ == nullptr);
  job_ = ptr;
  cv_.notify_all();
}

bool detached_scheduler::is_system_scheduler() const noexcept {
  return false;
}

void detached_scheduler::init(std::thread hdl) {
  thread_ = std::move(hdl);
}

void detached_scheduler::start() {
  // nop
}

void detached_scheduler::stop() {
  {
    std::unique_lock guard{mtx_};
    shutdown_ = true;
    cv_.notify_all();
  }
  if (thread_.joinable())
    thread_.join();
}

void detached_scheduler::run() {
  auto lg = log::core::trace("");
  for (;;) {
    auto job = await();
    if (job) {
      job->resume(this, resumable::default_event_id);
      intrusive_ptr_release(job);
    } else {
      // No job and shutdown_ is true, exit the loop.
      return;
    }
  }
}

resumable* detached_scheduler::await() {
  std::unique_lock guard{mtx_};
  while (job_ == nullptr && !shutdown_)
    cv_.wait(guard);
  auto ptr = job_;
  job_ = nullptr;
  return ptr;
}

} // namespace caf::internal
