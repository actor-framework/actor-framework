// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE message_lifetime

#include "caf/all.hpp"

#include "core-test.hpp"

#include <atomic>
#include <iostream>

using namespace caf;

fail_on_copy::fail_on_copy(const fail_on_copy&) {
  CAF_FAIL("fail_on_copy: copy constructor called");
}

fail_on_copy& fail_on_copy::operator=(const fail_on_copy&) {
  CAF_FAIL("fail_on_copy: copy assign operator called");
}

namespace {

class testee : public event_based_actor {
public:
  testee(actor_config& cfg) : event_based_actor(cfg) {
    // nop
  }

  ~testee() override {
    // nop
  }

  behavior make_behavior() override {
    // reflecting a message increases its reference count by one
    set_default_handler(reflect_and_quit);
    return {[] {
      // nop
    }};
  }
};

class tester : public event_based_actor {
public:
  tester(actor_config& cfg, actor aut)
    : event_based_actor(cfg),
      aut_(std::move(aut)),
      msg_(make_message(1, 2, 3)) {
    set_down_handler([this](down_msg& dm) {
      CHECK_EQ(dm.reason, exit_reason::normal);
      CHECK_EQ(dm.source, aut_.address());
      quit();
    });
  }

  behavior make_behavior() override {
    monitor(aut_);
    send(aut_, msg_);
    return {
      [](int a, int b, int c) {
        CHECK_EQ(a, 1);
        CHECK_EQ(b, 2);
        CHECK_EQ(c, 3);
      },
    };
  }

private:
  actor aut_;
  message msg_;
};

} // namespace

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

CAF_TEST(nocopy_in_scoped_actor) {
  auto msg = make_message(fail_on_copy{1});
  self->send(self, msg);
  self->receive([&](const fail_on_copy& x) {
    CHECK_EQ(x.value, 1);
    CHECK_EQ(msg.cdata().get_reference_count(), 2u);
  });
  CHECK_EQ(msg.cdata().get_reference_count(), 1u);
}

CAF_TEST(message_lifetime_in_scoped_actor) {
  auto msg = make_message(1, 2, 3);
  self->send(self, msg);
  self->receive([&](int a, int b, int c) {
    CHECK_EQ(a, 1);
    CHECK_EQ(b, 2);
    CHECK_EQ(c, 3);
    CHECK_EQ(msg.cdata().get_reference_count(), 2u);
  });
  CHECK_EQ(msg.cdata().get_reference_count(), 1u);
  msg = make_message(42);
  self->send(self, msg);
  CHECK_EQ(msg.cdata().get_reference_count(), 2u);
  self->receive([&](int& value) {
    auto addr = static_cast<void*>(&value);
    CHECK_NE(addr, msg.cdata().at(0));
    value = 10;
  });
  CHECK_EQ(msg.get_as<int>(0), 42);
}

CAF_TEST(message_lifetime_in_spawned_actor) {
  for (size_t i = 0; i < 100; ++i)
    sys.spawn<tester>(sys.spawn<testee>());
}

END_FIXTURE_SCOPE()
