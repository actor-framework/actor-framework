// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_registry.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/stateful_actor.hpp"

#include <chrono>

using namespace caf;
using namespace std::literals;

namespace {

struct pong_state {
  static inline const char* name = "pong";
};

behavior pong(stateful_actor<pong_state>*) {
  return {
    [](ping_atom) { //
      return pong_atom_v;
    },
  };
}

struct ping_state {
  static inline const char* name = "ping";
  bool had_first_timeout = false; // unused in ping_singleN functions
};

using ping_actor = stateful_actor<ping_state>;

using fptr = behavior (*)(ping_actor*, std::shared_ptr<bool>, const actor&);
using test_vec = std::vector<std::pair<fptr, std::string>>;

// assumes to receive a timeout (sent via delayed_send) before pong replies
behavior ping_single1(ping_actor* self, std::shared_ptr<bool> had_timeout,
                      const actor& buddy) {
  self->mail(ping_atom_v).send(buddy);
  self->mail(timeout_atom_v).delay(1s).send(self);
  return {
    [](pong_atom) { //
      test::runnable::current().fail("received pong atom");
    },
    [self, had_timeout](timeout_atom) {
      *had_timeout = true;
      self->quit();
    },
  };
}

// assumes to receive a timeout before pong replies
behavior ping_single2(ping_actor* self, std::shared_ptr<bool> had_timeout,
                      const actor& buddy) {
  self->mail(ping_atom_v).send(buddy);
  self->set_idle_handler(1s, strong_ref, once, [self, had_timeout] {
    auto lg = log::core::trace("had_timeout = {}", *had_timeout);
    *had_timeout = true;
    self->quit();
  });
  return {
    [](pong_atom) { test::runnable::current().fail("received pong atom"); }};
}

// assumes to receive a timeout (via request error handler) before pong replies
behavior ping_single3(ping_actor* self, std::shared_ptr<bool> had_timeout,
                      const actor& buddy) {
  self->request(buddy, 100ms, ping_atom_v)
    .then(
      [](pong_atom) { test::runnable::current().fail("received pong atom"); },
      [had_timeout](const error& err) {
        test::runnable::current().require(err == sec::request_timeout);
        *had_timeout = true;
      });
  return {}; // dummy value in order to give all 3 variants the same fun sig
}

// assumes to receive an inner timeout (sent via delayed_send) before pong
// replies, then second timeout fires
behavior ping_nested1(ping_actor* self, std::shared_ptr<bool> had_timeout,
                      const actor& buddy) {
  self->mail(ping_atom_v).send(buddy);
  self->mail(timeout_atom_v).delay(1s).send(self);
  return {
    [](pong_atom) { test::runnable::current().fail("received pong atom"); },
    [self, had_timeout](timeout_atom) {
      self->state().had_first_timeout = true;
      self->set_idle_handler(100ms, strong_ref, once, [self, had_timeout] {
        test::runnable::current().check(self->state().had_first_timeout);
        *had_timeout = true;
        self->quit();
      });
    },
  };
}

// assumes to receive an inner timeout  before pong replies, then a second
// timeout fires
behavior ping_nested2(ping_actor* self, std::shared_ptr<bool> had_timeout,
                      const actor& buddy) {
  self->mail(ping_atom_v).send(buddy);
  self->set_idle_handler(1s, strong_ref, once, [self, had_timeout] {
    self->state().had_first_timeout = true;
    self->set_idle_handler(100ms, strong_ref, once, [self, had_timeout] {
      test::runnable::current().check(self->state().had_first_timeout);
      *had_timeout = true;
      self->quit();
    });
  });
  return {
    [](pong_atom) { test::runnable::current().fail("received pong atom"); },
  };
}

// assumes to receive an inner timeout (via request error handler) before pong
// replies, then a second timeout fires
behavior ping_nested3(ping_actor* self, std::shared_ptr<bool> had_timeout,
                      const actor& buddy) {
  self->request(buddy, 100ms, ping_atom_v)
    .then(
      [self](pong_atom) {
        test::runnable::current().fail("received pong atom");
        self->quit(sec::unexpected_message);
      },
      [self, had_timeout](const error& err) {
        test::runnable::current().require_eq(err, sec::request_timeout);
        self->state().had_first_timeout = true;
        self->set_idle_handler(100ms, strong_ref, once, [self, had_timeout] {
          test::runnable::current().check(self->state().had_first_timeout);
          *had_timeout = true;
          self->quit();
        });
      });
  return {[] {}};
}

// uses .then on both requests
behavior ping_multiplexed1(ping_actor* self, std::shared_ptr<bool> had_timeout,
                           const actor& pong_actor) {
  self->request(pong_actor, 100ms, ping_atom_v)
    .then(
      [](pong_atom) { test::runnable::current().fail("received pong atom"); },
      [self, had_timeout](const error& err) {
        test::runnable::current().require_eq(err, sec::request_timeout);
        if (!self->state().had_first_timeout)
          self->state().had_first_timeout = true;
        else
          *had_timeout = true;
      });
  self->request(pong_actor, 100ms, ping_atom_v)
    .then(
      [](pong_atom) { test::runnable::current().fail("received pong atom"); },
      [self, had_timeout](const error& err) {
        test::runnable::current().require_eq(err, sec::request_timeout);
        if (!self->state().had_first_timeout)
          self->state().had_first_timeout = true;
        else
          *had_timeout = true;
      });
  return {};
}

// uses .await on both requests
behavior ping_multiplexed2(ping_actor* self, std::shared_ptr<bool> had_timeout,
                           const actor& pong_actor) {
  self->request(pong_actor, 100ms, ping_atom_v)
    .await(
      [](pong_atom) { test::runnable::current().fail("received pong atom"); },
      [self, had_timeout](const error& err) {
        test::runnable::current().require_eq(err, sec::request_timeout);
        if (!self->state().had_first_timeout)
          self->state().had_first_timeout = true;
        else
          *had_timeout = true;
      });
  self->request(pong_actor, 100ms, ping_atom_v)
    .await(
      [](pong_atom) { test::runnable::current().fail("received pong atom"); },
      [self, had_timeout](const error& err) {
        test::runnable::current().require_eq(err, sec::request_timeout);
        if (!self->state().had_first_timeout)
          self->state().had_first_timeout = true;
        else
          *had_timeout = true;
      });
  return {};
}

// uses .await and .then
behavior ping_multiplexed3(ping_actor* self, std::shared_ptr<bool> had_timeout,
                           const actor& pong_actor) {
  self->request(pong_actor, 100ms, ping_atom_v)
    .then(
      [](pong_atom) { test::runnable::current().fail("received pong atom"); },
      [self, had_timeout](const error& err) {
        test::runnable::current().require_eq(err, sec::request_timeout);
        if (!self->state().had_first_timeout)
          self->state().had_first_timeout = true;
        else
          *had_timeout = true;
      });
  self->request(pong_actor, 100ms, ping_atom_v)
    .await(
      [](pong_atom) { test::runnable::current().fail("received pong atom"); },
      [self, had_timeout](const error& err) {
        test::runnable::current().require_eq(err, sec::request_timeout);
        if (!self->state().had_first_timeout)
          self->state().had_first_timeout = true;
        else
          *had_timeout = true;
      });
  return {};
}

WITH_FIXTURE(test::fixture::deterministic) {

TEST("single timeout") {
  test_vec fs{{ping_single1, "ping_single1"},
              {ping_single2, "ping_single2"},
              {ping_single3, "ping_single3"}};
  for (auto f : fs) {
    auto had_timeout = std::make_shared<bool>(false);
    log::test::debug("test implementation {}", f.second);
    auto testee = sys.spawn(f.first, had_timeout, sys.spawn<lazy_init>(pong));
    require_eq(mail_count(), 1u);
    trigger_timeout();
    dispatch_message();
    require_eq(mail_count(), 2u);
    // now, the timeout message is already dispatched, while pong did
    // not respond to the message yet, i.e., timeout arrives before response
    dispatch_messages();
    check(*had_timeout);
  }
}

TEST("nested timeout") {
  auto had_timeout = std::make_shared<bool>(false);
  SECTION("idle timeout from a regular message handler") {
    auto testee = sys.spawn(ping_nested1, had_timeout,
                            sys.spawn<lazy_init>(pong));
    require_eq(mail_count(), 1u);
    // Trigger the timeout_atom message that we trigger manually.
    trigger_timeout();
    expect<timeout_atom>().to(testee);
    check(!*had_timeout);
    // Trigger the idle timeout.
    trigger_timeout();
    expect<timeout_msg>().to(testee);
    check(*had_timeout);
  }
  SECTION("idle timeout from another idle timeout") {
    auto testee = sys.spawn(ping_nested2, had_timeout,
                            sys.spawn<lazy_init>(pong));
    require_eq(mail_count(), 1u);
    // Trigger the timeout_atom message that we trigger manually.
    trigger_timeout();
    expect<timeout_msg>().to(testee);
    check(!*had_timeout);
    // Trigger the idle timeout.
    trigger_timeout();
    expect<timeout_msg>().to(testee);
    check(*had_timeout);
  }
  SECTION("idle timeout from a request timeout") {
    auto testee = sys.spawn(ping_nested3, had_timeout,
                            sys.spawn<lazy_init>(pong));
    require_eq(mail_count(), 1u);
    // Trigger the timeout_atom message that we trigger manually.
    trigger_timeout();
    expect<error>().to(testee);
    check(!*had_timeout);
    // Trigger the idle timeout.
    trigger_timeout();
    expect<timeout_msg>().to(testee);
    check(*had_timeout);
  }
}

TEST("multiplexed timeout") {
  test_vec fs{{ping_multiplexed1, "ping_multiplexed1"},
              {ping_multiplexed2, "ping_multiplexed2"},
              {ping_multiplexed3, "ping_multiplexed3"}};
  for (auto f : fs) {
    auto had_timeout = std::make_shared<bool>(false);
    log::test::debug("test implementation {}", f.second);
    auto testee = sys.spawn(f.first, had_timeout, sys.spawn<lazy_init>(pong));
    require_eq(mail_count(), 2u);
    trigger_all_timeouts();
    require_eq(mail_count(), 4u);
    // now, the timeout message is already dispatched, while pong did
    // not respond to the message yet, i.e., timeout arrives before response
    dispatch_messages();
    check(*had_timeout);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
