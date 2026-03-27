// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/cleanup_and_release.hpp"

#include "caf/detail/assert.hpp"
#include "caf/resumable.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/scheduler.hpp"

#include <vector>

namespace caf::detail {

void cleanup_and_release(resumable_ptr job) {
  CAF_ASSERT(job != nullptr);
  class dummy_scheduler : public scheduler {
  public:
    void delay(resumable_ptr sub, uint64_t) override {
      resumables.push_back(std::move(sub));
    }
    void schedule(resumable_ptr sub, uint64_t) override {
      resumables.push_back(std::move(sub));
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
    std::vector<resumable_ptr> resumables;
  };
  dummy_scheduler dummy;
  job->resume(&dummy, resumable::dispose_event_id);
  while (!dummy.resumables.empty()) {
    auto sub = std::move(dummy.resumables.back());
    dummy.resumables.pop_back();
    sub->resume(&dummy, resumable::dispose_event_id);
  }
}

} // namespace caf::detail
