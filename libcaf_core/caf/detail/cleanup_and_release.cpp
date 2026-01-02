// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/cleanup_and_release.hpp"

#include "caf/resumable.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/scheduler.hpp"

namespace caf::detail {

void cleanup_and_release(resumable* ptr) {
  class dummy_scheduler : public scheduler {
  public:
    void delay(resumable* job, uint64_t) override {
      resumables.push_back(job);
    }
    void schedule(resumable* job, uint64_t) override {
      resumables.push_back(job);
    }
    void start() override {
      // nop
    }
    void stop() override {
      // nop
    }
    bool is_system_scheduler() const noexcept final {
      return true;
    }
    std::vector<resumable*> resumables;
  };
  dummy_scheduler dummy;
  ptr->resume(&dummy, resumable::dispose_event_id);
  while (!dummy.resumables.empty()) {
    auto sub = dummy.resumables.back();
    dummy.resumables.pop_back();
    sub->resume(&dummy, resumable::dispose_event_id);
    intrusive_ptr_release(sub);
  }
  intrusive_ptr_release(ptr);
}

} // namespace caf::detail
