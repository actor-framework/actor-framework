// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
  switch (ptr->subtype()) {
    case resumable::scheduled_actor:
    case resumable::io_actor: {
      auto dptr = static_cast<scheduled_actor*>(ptr);
      dummy_scheduler dummy;
      dptr->cleanup(error{exit_reason::user_shutdown}, &dummy);
      while (!dummy.resumables.empty()) {
        auto sub = dummy.resumables.back();
        dummy.resumables.pop_back();
        switch (sub->subtype()) {
          case resumable::scheduled_actor:
          case resumable::io_actor: {
            auto dsub = static_cast<scheduled_actor*>(sub);
            dsub->cleanup(error{exit_reason::user_shutdown}, &dummy);
            break;
          }
          default:
            break;
        }
      }
      break;
    }
    default:
      break;
  }
  intrusive_ptr_release(ptr);
}

} // namespace caf::detail
