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

using std::chrono::milliseconds;
using std::chrono::seconds;

using namespace std::string_literals;

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

using fptr = behavior (*)(ping_actor*, bool*, const actor&);
using test_vec = std::vector<std::pair<fptr, std::string>>;

// assumes to receive a timeout (sent via delayed_send) before pong replies
behavior ping_single1(ping_actor* self, bool* had_timeout, const actor& buddy) {
  self->mail(ping_atom_v).send(buddy);
  self->mail(timeout_atom_v).delay(std::chrono::seconds(1)).send(self);
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

// assumes to receive a timeout (via after()) before pong replies
behavior ping_single2(ping_actor* self, bool* had_timeout, const actor& buddy) {
  self->mail(ping_atom_v).send(buddy);
  return {
    [](pong_atom) { test::runnable::current().fail("received pong atom"); },
    after(std::chrono::seconds(1)) >>
      [self, had_timeout] {
        auto lg = log::core::trace("had_timeout = {}", *had_timeout);
        *had_timeout = true;
        self->quit();
      },
  };
}

// assumes to receive a timeout (via request error handler) before pong replies
behavior ping_single3(ping_actor* self, bool* had_timeout, const actor& buddy) {
  self->request(buddy, milliseconds(100), ping_atom_v)
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
behavior ping_nested1(ping_actor* self, bool* had_timeout, const actor& buddy) {
  self->mail(ping_atom_v).send(buddy);
  self->mail(timeout_atom_v).delay(std::chrono::seconds(1)).send(self);
  return {
    [](pong_atom) { test::runnable::current().fail("received pong atom"); },
    [self, had_timeout](timeout_atom) {
      self->state().had_first_timeout = true;
      self->become(after(milliseconds(100)) >> [self, had_timeout] {
        test::runnable::current().check(self->state().had_first_timeout);
        *had_timeout = true;
        self->quit();
      });
    },
  };
}

// assumes to receive an inner timeout (via after()) before pong replies, then a
// second timeout fires
behavior ping_nested2(ping_actor* self, bool* had_timeout, const actor& buddy) {
  self->mail(ping_atom_v).send(buddy);
  return {
    [](pong_atom) { test::runnable::current().fail("received pong atom"); },
    after(std::chrono::seconds(1)) >>
      [self, had_timeout] {
        self->state().had_first_timeout = true;
        self->become(after(milliseconds(100)) >> [self, had_timeout] {
          test::runnable::current().check(self->state().had_first_timeout);
          *had_timeout = true;
          self->quit();
        });
      },
  };
}

// assumes to receive an inner timeout (via request error handler) before pong
// replies, then a second timeout fires
behavior ping_nested3(ping_actor* self, bool* had_timeout, const actor& buddy) {
  self->request(buddy, milliseconds(100), ping_atom_v)
    .then(
      [self](pong_atom) {
        test::runnable::current().fail("received pong atom");
        self->quit(sec::unexpected_message);
      },
      [self](const error& err) {
        test::runnable::current().require_eq(err, sec::request_timeout);
        self->state().had_first_timeout = true;
      });
  return {
    after(milliseconds(100)) >>
      [self, had_timeout] {
        test::runnable::current().check(self->state().had_first_timeout);
        *had_timeout = true;
        self->quit();
      },
  };
}

// uses .then on both requests
behavior ping_multiplexed1(ping_actor* self, bool* had_timeout,
                           const actor& pong_actor) {
  self->request(pong_actor, milliseconds(100), ping_atom_v)
    .then(
      [](pong_atom) { test::runnable::current().fail("received pong atom"); },
      [self, had_timeout](const error& err) {
        test::runnable::current().require_eq(err, sec::request_timeout);
        if (!self->state().had_first_timeout)
          self->state().had_first_timeout = true;
        else
          *had_timeout = true;
      });
  self->request(pong_actor, milliseconds(100), ping_atom_v)
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
behavior ping_multiplexed2(ping_actor* self, bool* had_timeout,
                           const actor& pong_actor) {
  self->request(pong_actor, milliseconds(100), ping_atom_v)
    .await(
      [](pong_atom) { test::runnable::current().fail("received pong atom"); },
      [self, had_timeout](const error& err) {
        test::runnable::current().require_eq(err, sec::request_timeout);
        if (!self->state().had_first_timeout)
          self->state().had_first_timeout = true;
        else
          *had_timeout = true;
      });
  self->request(pong_actor, milliseconds(100), ping_atom_v)
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
behavior ping_multiplexed3(ping_actor* self, bool* had_timeout,
                           const actor& pong_actor) {
  self->request(pong_actor, milliseconds(100), ping_atom_v)
    .then(
      [](pong_atom) { test::runnable::current().fail("received pong atom"); },
      [self, had_timeout](const error& err) {
        test::runnable::current().require_eq(err, sec::request_timeout);
        if (!self->state().had_first_timeout)
          self->state().had_first_timeout = true;
        else
          *had_timeout = true;
      });
  self->request(pong_actor, milliseconds(100), ping_atom_v)
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

TEST("single_timeout") {
  test_vec fs{{ping_single1, "ping_single1"},
              {ping_single2, "ping_single2"},
              {ping_single3, "ping_single3"}};
  for (auto f : fs) {
    bool had_timeout = false;
    log::test::debug("test implementation {}", f.second);
    auto testee = sys.spawn(f.first, &had_timeout, sys.spawn<lazy_init>(pong));
    require_eq(mail_count(), 1u);
    require_eq(next_message<local_actor>().name(), "pong"s);
    trigger_timeout();
    dispatch_message();
    require_eq(mail_count(), 2u);
    // now, the timeout message is already dispatched, while pong did
    // not respond to the message yet, i.e., timeout arrives before response
    dispatch_messages();
    check(had_timeout);
  }
}

TEST("nested_timeout") {
  test_vec fs{{ping_nested1, "ping_nested1"},
              {ping_nested2, "ping_nested2"},
              {ping_nested3, "ping_nested3"}};
  for (auto f : fs) {
    bool had_timeout = false;
    log::test::debug("test implementation {}", f.second);
    auto testee = sys.spawn(f.first, &had_timeout, sys.spawn<lazy_init>(pong));
    require_eq(mail_count(), 1u);
    require_eq(next_message<local_actor>().name(), "pong"s);
    trigger_timeout();
    require_eq(mail_count(), 2u);
    // now, the timeout message is already dispatched, while pong did
    // not respond to the message yet, i.e., timeout arrives before response
    dispatch_messages();
    // dispatch second timeout
    require_eq(trigger_timeout(), true);
    require_eq(next_message<local_actor>().name(), "ping"s);
    check(!had_timeout);
    check(next_message<ping_actor>().state().had_first_timeout);
    dispatch_messages();
    check(had_timeout);
  }
}

TEST("multiplexed_timeout") {
  test_vec fs{{ping_multiplexed1, "ping_multiplexed1"},
              {ping_multiplexed2, "ping_multiplexed2"},
              {ping_multiplexed3, "ping_multiplexed3"}};
  for (auto f : fs) {
    bool had_timeout = false;
    log::test::debug("test implementation {}", f.second);
    auto testee = sys.spawn(f.first, &had_timeout, sys.spawn<lazy_init>(pong));
    require_eq(mail_count(), 2u);
    require_eq(next_message<local_actor>().name(), "pong"s);
    trigger_all_timeouts();
    require_eq(mail_count(), 4u);
    // now, the timeout message is already dispatched, while pong did
    // not respond to the message yet, i.e., timeout arrives before response
    dispatch_messages();
    check(had_timeout);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
