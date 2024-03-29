// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

namespace {

behavior mirror_impl(event_based_actor* self) {
  self->set_default_handler(reflect);
  return [] {
    // nop
  };
}

struct fixture : test::fixture::deterministic {
  actor mirror;
  actor testee;
  scoped_actor self{sys};

  fixture() {
    mirror = sys.spawn(mirror_impl);
    // run initialization code or mirror
    dispatch_message();
  }

  template <class... Ts>
  void spawn(Ts&&... xs) {
    testee = self->spawn(std::forward<Ts>(xs)...);
  }

  ~fixture() {
    self->wait_for(testee);
  }
};

WITH_FIXTURE(fixture) {

TEST("single_multiplexed_request") {
  auto f = [&](event_based_actor* self, actor server) {
    self->mail(42).request(server, infinite).then([this](int x) {
      auto lg = log::core::trace("x = {}", x);
      require_eq(x, 42);
    });
  };
  spawn(f, mirror);
  // run initialization code of testee
  expect<int>().with(42).from(testee).to(mirror);
  expect<int>().with(42).from(mirror).to(testee);
}

TEST("multiple_multiplexed_requests") {
  auto f = [&](event_based_actor* self, actor server) {
    for (int i = 0; i < 3; ++i)
      self->mail(42).request(server, infinite).then([this](int x) {
        auto lg = log::core::trace("x = {}", x);
        require_eq(x, 42);
      });
  };
  spawn(f, mirror);
  // run initialization code of testee
  expect<int>().with(42).from(testee).to(mirror);
  expect<int>().with(42).from(testee).to(mirror);
  expect<int>().with(42).from(testee).to(mirror);
  expect<int>().with(42).from(mirror).to(testee);
  expect<int>().with(42).from(mirror).to(testee);
  expect<int>().with(42).from(mirror).to(testee);
}

TEST("single_awaited_request") {
  auto f = [&](event_based_actor* self, actor server) {
    self->mail(42).request(server, infinite).await([this](int x) {
      require_eq(x, 42);
    });
  };
  spawn(f, mirror);
  // run initialization code of testee
  expect<int>().with(42).from(testee).to(mirror);
  expect<int>().with(42).from(mirror).to(testee);
}

TEST("multiple_awaited_requests") {
  auto f = [&](event_based_actor* self, actor server) {
    for (int i = 0; i < 3; ++i)
      self->mail(i).request(server, infinite).await([this, i](int x) {
        log::test::debug("received response #{}", (i + 1));
        require_eq(x, i);
      });
  };
  spawn(f, mirror);
  // run initialization code of testee
  self->monitor(testee);
  expect<int>().with(0).from(testee).to(mirror);
  expect<int>().with(1).from(testee).to(mirror);
  expect<int>().with(2).from(testee).to(mirror);
  // request().await() processes messages out-of-order,
  // which means we cannot check using expect()
  dispatch_messages();
  auto received_down_msg = std::make_shared<bool>(false);
  self->receive([&received_down_msg](down_msg&) { *received_down_msg = true; });
  check(*received_down_msg);
}

} // WITH_FIXTURE(fixture)

} // namespace
