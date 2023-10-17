// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/mailbox_factory.hpp"

#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/detail/default_mailbox.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/local_actor.hpp"
#include "caf/scheduler/test_coordinator.hpp"

using namespace caf;

namespace {

class dummy_mailbox_factory : public detail::mailbox_factory {
public:
  ~dummy_mailbox_factory() override {
    for (auto& kvp : mailboxes)
      kvp.second->deref_mailbox();
  }

  abstract_mailbox* make(local_actor* owner) {
    auto ptr = new detail::default_mailbox;
    ptr->ref_mailbox();
    mailboxes.emplace(owner->id(), ptr);
    return ptr;
  }

  abstract_mailbox* make(scheduled_actor* owner) override {
    return make(static_cast<local_actor*>(owner));
  }

  abstract_mailbox* make(blocking_actor* owner) override {
    return make(static_cast<local_actor*>(owner));
  }

  std::map<actor_id, detail::default_mailbox*> mailboxes;
};

TEST("a mailbox factory creates mailboxes for actors") {
  dummy_mailbox_factory factory;
  actor_system_config cfg;
  cfg.set("caf.scheduler.policy", "testing");
  actor_system sys{cfg};
  auto& sched = static_cast<scheduler::test_coordinator&>(sys.scheduler());
  auto spawn_dummy = [&sys, &factory](auto fn) {
    using impl_t = event_based_actor;
    actor_config cfg{sys.dummy_execution_unit()};
    cfg.init_fun.emplace([fn](local_actor* self) {
      fn(static_cast<impl_t*>(self));
      return behavior{};
    });
    cfg.mbox_factory = &factory;
    auto res = make_actor<impl_t>(sys.next_actor_id(), sys.node(), &sys, cfg);
    auto ptr = static_cast<impl_t*>(actor_cast<abstract_actor*>(res));
    ptr->launch(cfg.host, false, false);
    return res;
  };
  SECTION("spawning dummies creates mailboxes") {
    auto initialized = std::make_shared<size_t>(0u);
    auto dummy_impl = [this, &factory, initialized](event_based_actor* self) {
      ++*initialized;
      auto* mbox = factory.mailboxes[self->id()];
      check_eq(std::addressof(self->mailbox()), mbox);
      check_eq(mbox->ref_count(), 2u);
    };
    check_eq(factory.mailboxes.size(), 0u);
    auto dummy1 = spawn_dummy(dummy_impl).id();
    check_eq(factory.mailboxes.size(), 1u);
    auto dummy2 = spawn_dummy(dummy_impl).id();
    check_eq(factory.mailboxes.size(), 2u);
    sched.run();
    check_eq(*initialized, 2u);
    check_eq(factory.mailboxes[dummy1]->ref_count(), 1u);
    check_eq(factory.mailboxes[dummy2]->ref_count(), 1u);
  }
}

} // namespace
