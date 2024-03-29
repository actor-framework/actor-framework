// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/log/test.hpp"

using namespace caf;

namespace {

class testee : public event_based_actor {
public:
  testee(actor_config& cfg, actor buddy) : event_based_actor(cfg) {
    attach_functor([this, buddy](const error& rsn) { //
      mail(ok_atom_v, rsn).send(buddy);
    });
  }

  behavior make_behavior() override {
    return {
      [this](delete_atom) {
        log::test::debug("testee received delete");
        quit(exit_reason::user_shutdown);
      },
    };
  }
};

class spawner : public event_based_actor {
public:
  spawner(actor_config& cfg) : event_based_actor(cfg), downs_(0) {
    set_down_handler([this](down_msg& msg) {
      test::runnable::current().check_eq(msg.reason,
                                         exit_reason::user_shutdown);
      test::runnable::current().check_eq(msg.source, testee_.address());
      if (++downs_ == 2)
        quit(msg.reason);
    });
  }

  behavior make_behavior() override {
    testee_ = spawn<testee, monitored>(this);
    return {
      [this](ok_atom, const error& reason) {
        test::runnable::current().check_eq(reason, exit_reason::user_shutdown);
        if (++downs_ == 2) {
          quit(reason);
        }
      },
      [this](delete_atom x) {
        log::test::debug("spawner received delete");
        return delegate(testee_, x);
      },
    };
  }

  void on_exit() override {
    testee_ = nullptr;
  }

private:
  int downs_;
  actor testee_;
};

WITH_FIXTURE(test::fixture::deterministic) {

TEST("constructor_attach") {
  anon_send(sys.spawn<spawner>(), delete_atom_v);
  dispatch_messages();
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
