// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE constructor_attach

#include "caf/all.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

class testee : public event_based_actor {
public:
  testee(actor_config& cfg, actor buddy) : event_based_actor(cfg) {
    attach_functor(
      [this, buddy](const error& rsn) { send(buddy, ok_atom_v, rsn); });
  }

  behavior make_behavior() override {
    return {
      [this](delete_atom) {
        MESSAGE("testee received delete");
        quit(exit_reason::user_shutdown);
      },
    };
  }
};

class spawner : public event_based_actor {
public:
  spawner(actor_config& cfg) : event_based_actor(cfg), downs_(0) {
    set_down_handler([this](down_msg& msg) {
      CHECK_EQ(msg.reason, exit_reason::user_shutdown);
      CHECK_EQ(msg.source, testee_.address());
      if (++downs_ == 2)
        quit(msg.reason);
    });
  }

  behavior make_behavior() override {
    testee_ = spawn<testee, monitored>(this);
    return {
      [this](ok_atom, const error& reason) {
        CHECK_EQ(reason, exit_reason::user_shutdown);
        if (++downs_ == 2) {
          quit(reason);
        }
      },
      [this](delete_atom x) {
        MESSAGE("spawner received delete");
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

} // namespace

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

CAF_TEST(constructor_attach) {
  anon_send(sys.spawn<spawner>(), delete_atom_v);
  run();
}

END_FIXTURE_SCOPE()
