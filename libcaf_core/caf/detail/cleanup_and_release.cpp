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
    void delay(resumable* job) override {
      resumables.push_back(job);
    }
    void schedule(resumable* job) override {
      resumables.push_back(job);
    }
    void start() override {
      // nop
    }
    void stop() override {
      // nop
    }
    std::vector<resumable*> resumables;
  };
  dummy_scheduler dummy;
  std::ignore = ptr->resume(&dummy, resumable::dispose_event_id);
  intrusive_ptr_release(ptr);
  while (!dummy.resumables.empty()) {
    auto sub = dummy.resumables.back();
    dummy.resumables.pop_back();
    std::ignore = sub->resume(&dummy, resumable::dispose_event_id);
    intrusive_ptr_release(sub);
  }
}

} // namespace caf::detail
