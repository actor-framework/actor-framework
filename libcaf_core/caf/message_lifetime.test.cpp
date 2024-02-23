// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/all.hpp"

#include <atomic>
#include <iostream>

using namespace caf;

struct fail_on_copy {
  int value;

  fail_on_copy() : value(0) {
    // nop
  }

  explicit fail_on_copy(int x) : value(x) {
    // nop
  }

  fail_on_copy(fail_on_copy&&) = default;

  fail_on_copy& operator=(fail_on_copy&&) = default;

  fail_on_copy(const fail_on_copy&) {
    test::runnable::current().fail("fail_on_copy: copy constructor called");
  }

  fail_on_copy& operator=(const fail_on_copy&) {
    test::runnable::current().fail("fail_on_copy: copy assign operator called");
  }

  template <class Inspector>
  friend bool inspect(Inspector& f, fail_on_copy& x) {
    return f.object(x).fields(f.field("value", x.value));
  }
};

CAF_BEGIN_TYPE_ID_BLOCK(message_lifetime_test, caf::first_custom_type_id + 70)

  CAF_ADD_TYPE_ID(message_lifetime_test, (fail_on_copy))

CAF_END_TYPE_ID_BLOCK(message_lifetime_test)

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
      auto& this_test = test::runnable::current();
      this_test.check_eq(dm.reason, exit_reason::normal);
      this_test.check_eq(dm.source, aut_.address());
      quit();
    });
  }

  behavior make_behavior() override {
    monitor(aut_);
    mail(msg_).send(aut_);
    return {
      [](int a, int b, int c) {
        auto& this_test = test::runnable::current();
        this_test.check_eq(a, 1);
        this_test.check_eq(b, 2);
        this_test.check_eq(c, 3);
      },
    };
  }

private:
  actor aut_;
  message msg_;
};

struct fixture : test::fixture::deterministic {
  scoped_actor self{sys};
};

WITH_FIXTURE(fixture) {

TEST("nocopy_in_scoped_actor") {
  auto msg = make_message(fail_on_copy{1});
  self->mail(msg).send(self);
  self->receive([&](const fail_on_copy& x) {
    check_eq(x.value, 1);
    check_eq(msg.cdata().get_reference_count(), 2u);
  });
  check_eq(msg.cdata().get_reference_count(), 1u);
}

TEST("message_lifetime_in_scoped_actor") {
  auto msg = make_message(1, 2, 3);
  self->mail(msg).send(self);
  self->receive([&](int a, int b, int c) {
    check_eq(a, 1);
    check_eq(b, 2);
    check_eq(c, 3);
    check_eq(msg.cdata().get_reference_count(), 2u);
  });
  check_eq(msg.cdata().get_reference_count(), 1u);
  msg = make_message(42);
  self->mail(msg).send(self);
  check_eq(msg.cdata().get_reference_count(), 2u);
  self->receive([&](int& value) {
    auto addr = static_cast<void*>(&value);
    check_ne(addr, msg.cdata().at(0));
    value = 10;
  });
  check_eq(msg.get_as<int>(0), 42);
}

TEST("message_lifetime_in_spawned_actor") {
  for (size_t i = 0; i < 100; ++i)
    sys.spawn<tester>(sys.spawn<testee>());
}

TEST_INIT() {
  init_global_meta_objects<id_block::message_lifetime_test>();
}

} // WITH_FIXTURE(fixture)

} // namespace
