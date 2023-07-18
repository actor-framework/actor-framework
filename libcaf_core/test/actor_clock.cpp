// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE actor_clock

#include "caf/actor_clock.hpp"

#include "caf/all.hpp"
#include "caf/detail/test_actor_clock.hpp"

#include "core-test.hpp"

#include <chrono>
#include <memory>

using namespace caf;

using namespace std::chrono_literals;

namespace {

struct testee_state {
  event_based_actor* self;
  disposable pending;
  bool run_delayed_called = false;

  testee_state(event_based_actor* self) : self(self) {
    // nop
  }

  behavior make_behavior() {
    self->set_exit_handler([this](exit_msg& x) { self->quit(x.reason); });
    self->set_error_handler([](scheduled_actor*, error&) {});
    return {
      [this](ok_atom) {
        CAF_LOG_TRACE("" << self->current_mailbox_element()->content());
        pending = self->run_delayed(10s, [this] { run_delayed_called = true; });
      },
      [](const std::string&) { CAF_LOG_TRACE(""); },
    };
  }
};

using testee_actor = stateful_actor<testee_state>;

struct fixture : test_coordinator_fixture<> {
  detail::test_actor_clock& t;
  actor aut;

  fixture() : t(sched.clock()), aut(sys.spawn<testee_actor, lazy_init>()) {
    // nop
  }

  auto& state() {
    return deref<testee_actor>(aut).state;
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(run_delayed without dispose) {
  // Have AUT call self->run_delayed().
  self->send(aut, ok_atom_v);
  expect((ok_atom), from(self).to(aut).with(_));
  CHECK_EQ(t.actions.size(), 1u);
  // Advance time to trigger timeout.
  t.advance_time(10s);
  CHECK_EQ(t.actions.size(), 0u);
  // Have AUT receive the action.
  expect((action), to(aut));
  CHECK(state().run_delayed_called);
}

CAF_TEST(run_delayed with dispose before expire) {
  // Have AUT call self->run_delayed().
  self->send(aut, ok_atom_v);
  expect((ok_atom), from(self).to(aut).with(_));
  state().pending.dispose();
  CHECK_EQ(t.actions.size(), 1u);
  // Advance time, but the clock drops the disposed callback.
  t.advance_time(10s);
  CHECK_EQ(t.actions.size(), 0u);
  // Have AUT receive the timeout.
  disallow((action), to(aut));
  CHECK(!state().run_delayed_called);
}

CAF_TEST(run_delayed with dispose after expire) {
  // Have AUT call self->run_delayed().
  self->send(aut, ok_atom_v);
  expect((ok_atom), from(self).to(aut).with(_));
  CHECK_EQ(t.actions.size(), 1u);
  // Advance time to send timeout message.
  t.advance_time(10s);
  CHECK_EQ(t.actions.size(), 0u);
  // Have AUT receive the timeout but dispose it: turns into a nop.
  state().pending.dispose();
  expect((action), to(aut));
  CHECK(!state().run_delayed_called);
}

CAF_TEST(delay_actor_message) {
  // Schedule a message for now + 10s.
  auto n = t.now() + 10s;
  auto autptr = actor_cast<strong_actor_ptr>(aut);
  t.schedule_message(n, autptr,
                     make_mailbox_element(autptr, make_message_id(), no_stages,
                                          "foo"));
  CHECK_EQ(t.actions.size(), 1u);
  // Advance time to send the message.
  t.advance_time(10s);
  CHECK_EQ(t.actions.size(), 0u);
  // Have AUT receive the message.
  expect((std::string), from(aut).to(aut).with("foo"));
}

END_FIXTURE_SCOPE()
