// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE request_timeout

#include "caf/all.hpp"

#include "core-test.hpp"

#include <chrono>

CAF_PUSH_DEPRECATED_WARNING

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
    [=](ping_atom) { return pong_atom_v; },
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
    [=](pong_atom) { CAF_FAIL("received pong atom"); },
    [=](timeout_atom) {
      *had_timeout = true;
      self->quit();
    },
  };
}

// assumes to receive a timeout (via after()) before pong replies
behavior ping_single2(ping_actor* self, bool* had_timeout, const actor& buddy) {
  self->mail(ping_atom_v).send(buddy);
  return {
    [=](pong_atom) { CAF_FAIL("received pong atom"); },
    after(std::chrono::seconds(1)) >>
      [=] {
        auto lg = log::core::trace("had_timeout = {}", *had_timeout);
        *had_timeout = true;
        self->quit();
      },
  };
}

// assumes to receive a timeout (via request error handler) before pong replies
behavior ping_single3(ping_actor* self, bool* had_timeout, const actor& buddy) {
  self->request(buddy, milliseconds(100), ping_atom_v)
    .then([=](pong_atom) { CAF_FAIL("received pong atom"); },
          [=](const error& err) {
            CAF_REQUIRE(err == sec::request_timeout);
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
    [=](pong_atom) { CAF_FAIL("received pong atom"); },
    [=](timeout_atom) {
      self->state.had_first_timeout = true;
      self->become(after(milliseconds(100)) >> [=] {
        CHECK(self->state.had_first_timeout);
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
    [=](pong_atom) { CAF_FAIL("received pong atom"); },
    after(std::chrono::seconds(1)) >>
      [=] {
        self->state.had_first_timeout = true;
        self->become(after(milliseconds(100)) >> [=] {
          CHECK(self->state.had_first_timeout);
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
      [=](pong_atom) {
        CAF_FAIL("received pong atom");
        self->quit(sec::unexpected_message);
      },
      [=](const error& err) {
        CAF_REQUIRE_EQUAL(err, sec::request_timeout);
        self->state.had_first_timeout = true;
      });
  return {
    after(milliseconds(100)) >>
      [=] {
        CHECK(self->state.had_first_timeout);
        *had_timeout = true;
        self->quit();
      },
  };
}

// uses .then on both requests
behavior ping_multiplexed1(ping_actor* self, bool* had_timeout,
                           const actor& pong_actor) {
  self->request(pong_actor, milliseconds(100), ping_atom_v)
    .then([=](pong_atom) { CAF_FAIL("received pong atom"); },
          [=](const error& err) {
            CAF_REQUIRE_EQUAL(err, sec::request_timeout);
            if (!self->state.had_first_timeout)
              self->state.had_first_timeout = true;
            else
              *had_timeout = true;
          });
  self->request(pong_actor, milliseconds(100), ping_atom_v)
    .then([=](pong_atom) { CAF_FAIL("received pong atom"); },
          [=](const error& err) {
            CAF_REQUIRE_EQUAL(err, sec::request_timeout);
            if (!self->state.had_first_timeout)
              self->state.had_first_timeout = true;
            else
              *had_timeout = true;
          });
  return {};
}

// uses .await on both requests
behavior ping_multiplexed2(ping_actor* self, bool* had_timeout,
                           const actor& pong_actor) {
  self->request(pong_actor, milliseconds(100), ping_atom_v)
    .await([=](pong_atom) { CAF_FAIL("received pong atom"); },
           [=](const error& err) {
             CAF_REQUIRE_EQUAL(err, sec::request_timeout);
             if (!self->state.had_first_timeout)
               self->state.had_first_timeout = true;
             else
               *had_timeout = true;
           });
  self->request(pong_actor, milliseconds(100), ping_atom_v)
    .await([=](pong_atom) { CAF_FAIL("received pong atom"); },
           [=](const error& err) {
             CAF_REQUIRE_EQUAL(err, sec::request_timeout);
             if (!self->state.had_first_timeout)
               self->state.had_first_timeout = true;
             else
               *had_timeout = true;
           });
  return {};
}

// uses .await and .then
behavior ping_multiplexed3(ping_actor* self, bool* had_timeout,
                           const actor& pong_actor) {
  self->request(pong_actor, milliseconds(100), ping_atom_v)
    .then([=](pong_atom) { CAF_FAIL("received pong atom"); },
          [=](const error& err) {
            CAF_REQUIRE_EQUAL(err, sec::request_timeout);
            if (!self->state.had_first_timeout)
              self->state.had_first_timeout = true;
            else
              *had_timeout = true;
          });
  self->request(pong_actor, milliseconds(100), ping_atom_v)
    .await([=](pong_atom) { CAF_FAIL("received pong atom"); },
           [=](const error& err) {
             CAF_REQUIRE_EQUAL(err, sec::request_timeout);
             if (!self->state.had_first_timeout)
               self->state.had_first_timeout = true;
             else
               *had_timeout = true;
           });
  return {};
}

} // namespace

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

CAF_TEST(single_timeout) {
  test_vec fs{{ping_single1, "ping_single1"},
              {ping_single2, "ping_single2"},
              {ping_single3, "ping_single3"}};
  for (auto f : fs) {
    bool had_timeout = false;
    MESSAGE("test implementation " << f.second);
    auto testee = sys.spawn(f.first, &had_timeout, sys.spawn<lazy_init>(pong));
    CAF_REQUIRE_EQUAL(sched.jobs.size(), 1u);
    CAF_REQUIRE_EQUAL(sched.next_job<local_actor>().name(), "ping"s);
    sched.run_once();
    CAF_REQUIRE_EQUAL(sched.jobs.size(), 1u);
    CAF_REQUIRE_EQUAL(sched.next_job<local_actor>().name(), "pong"s);
    sched.trigger_timeout();
    CAF_REQUIRE_EQUAL(sched.jobs.size(), 2u);
    // now, the timeout message is already dispatched, while pong did
    // not respond to the message yet, i.e., timeout arrives before response
    CHECK_EQ(sched.run(), 2u);
    CHECK(had_timeout);
  }
}

CAF_TEST(nested_timeout) {
  test_vec fs{{ping_nested1, "ping_nested1"},
              {ping_nested2, "ping_nested2"},
              {ping_nested3, "ping_nested3"}};
  for (auto f : fs) {
    bool had_timeout = false;
    MESSAGE("test implementation " << f.second);
    auto testee = sys.spawn(f.first, &had_timeout, sys.spawn<lazy_init>(pong));
    CAF_REQUIRE_EQUAL(sched.jobs.size(), 1u);
    CAF_REQUIRE_EQUAL(sched.next_job<local_actor>().name(), "ping"s);
    sched.run_once();
    CAF_REQUIRE_EQUAL(sched.jobs.size(), 1u);
    CAF_REQUIRE_EQUAL(sched.next_job<local_actor>().name(), "pong"s);
    sched.trigger_timeout();
    CAF_REQUIRE_EQUAL(sched.jobs.size(), 2u);
    // now, the timeout message is already dispatched, while pong did
    // not respond to the message yet, i.e., timeout arrives before response
    sched.run();
    // dispatch second timeout
    CAF_REQUIRE_EQUAL(sched.trigger_timeout(), true);
    CAF_REQUIRE_EQUAL(sched.next_job<local_actor>().name(), "ping"s);
    CHECK(!had_timeout);
    CHECK(sched.next_job<ping_actor>().state.had_first_timeout);
    sched.run();
    CHECK(had_timeout);
  }
}

CAF_TEST(multiplexed_timeout) {
  test_vec fs{{ping_multiplexed1, "ping_multiplexed1"},
              {ping_multiplexed2, "ping_multiplexed2"},
              {ping_multiplexed3, "ping_multiplexed3"}};
  for (auto f : fs) {
    bool had_timeout = false;
    MESSAGE("test implementation " << f.second);
    auto testee = sys.spawn(f.first, &had_timeout, sys.spawn<lazy_init>(pong));
    CAF_REQUIRE_EQUAL(sched.jobs.size(), 1u);
    CAF_REQUIRE_EQUAL(sched.next_job<local_actor>().name(), "ping"s);
    sched.run_once();
    CAF_REQUIRE_EQUAL(sched.jobs.size(), 1u);
    CAF_REQUIRE_EQUAL(sched.next_job<local_actor>().name(), "pong"s);
    sched.trigger_timeouts();
    CAF_REQUIRE_EQUAL(sched.jobs.size(), 2u);
    // now, the timeout message is already dispatched, while pong did
    // not respond to the message yet, i.e., timeout arrives before response
    sched.run();
    CHECK(had_timeout);
  }
}

END_FIXTURE_SCOPE()

CAF_POP_WARNINGS
