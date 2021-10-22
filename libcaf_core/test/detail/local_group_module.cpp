// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.local_group_module

#include "caf/all.hpp"

#include "core-test.hpp"

#include <algorithm>
#include <array>
#include <chrono>

#include "caf/detail/local_group_module.hpp"

using namespace caf;

namespace {

struct testee_state {
  int x = 0;
  static inline const char* name = "testee";
};

behavior testee_impl(stateful_actor<testee_state>* self) {
  return {
    [=](put_atom, int x) { self->state.x = x; },
    [=](get_atom) { return self->state.x; },
  };
}

struct fixture : test_coordinator_fixture<> {
  fixture() {
    auto ptr = sys.groups().get_module("local");
    uut.reset(dynamic_cast<detail::local_group_module*>(ptr.get()));
  }

  ~fixture() {
    // Groups keep their subscribers alive (on purpose). Since we don't want to
    // manually kill all our testee actors, we simply force the group module to
    // stop here.
    uut->stop();
  }

  intrusive_ptr<detail::local_group_module> uut;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(local groups are singletons) {
  auto ptr1 = unbox(uut->get("test"));
  auto ptr2 = unbox(uut->get("test"));
  CHECK_EQ(ptr1.get(), ptr2.get());
  auto ptr3 = sys.groups().get_local("test");
  CHECK_EQ(ptr1.get(), ptr3.get());
}

CAF_TEST(local groups forward messages to all subscribers) {
  MESSAGE("Given two subscribers to the group 'test'.");
  auto grp = unbox(uut->get("test"));
  auto t1 = sys.spawn_in_group(grp, testee_impl);
  auto t2 = sys.spawn_in_group(grp, testee_impl);
  { // Subtest.
    MESSAGE("When an actors sends to the group.");
    self->send(grp, put_atom_v, 42);
    MESSAGE("Then both subscribers receive the message.");
    expect((put_atom, int), from(self).to(t1).with(_, 42));
    expect((put_atom, int), from(self).to(t2).with(_, 42));
  }
  { // Subtest.
    MESSAGE("When an actors leaves the group.");
    MESSAGE("And an actors sends to the group.");
    grp->unsubscribe(actor_cast<actor_control_block*>(t1));
    self->send(grp, put_atom_v, 23);
    MESSAGE("Then only one remaining actor receives the message.");
    disallow((put_atom, int), from(self).to(t1).with(_, 23));
    expect((put_atom, int), from(self).to(t2).with(_, 23));
  }
}

CAF_TEST(local group intermediaries manage groups) {
  MESSAGE("Given two subscribers to the group 'test'.");
  auto grp = unbox(uut->get("test"));
  auto intermediary = grp.get()->intermediary();
  auto t1 = sys.spawn_in_group(grp, testee_impl);
  auto t2 = sys.spawn_in_group(grp, testee_impl);
  { // Subtest.
    MESSAGE("When an actors sends to the group's intermediary.");
    inject((forward_atom, message),
           from(self)
             .to(intermediary)
             .with(forward_atom_v, make_message(put_atom_v, 42)));
    MESSAGE("Then both subscribers receive the message.");
    expect((put_atom, int), from(self).to(t1).with(_, 42));
    expect((put_atom, int), from(self).to(t2).with(_, 42));
  }
  auto t3 = sys.spawn(testee_impl);
  { // Subtest.
    MESSAGE("When an actor sends 'join' to the group's intermediary.");
    MESSAGE("And an actors sends to the group's intermediary.");
    inject((join_atom, strong_actor_ptr),
           from(self)
             .to(intermediary)
             .with(join_atom_v, actor_cast<strong_actor_ptr>(t3)));
    self->send(grp, put_atom_v, 23);
    MESSAGE("Then all three subscribers receive the message.");
    expect((put_atom, int), from(self).to(t1).with(_, 23));
    expect((put_atom, int), from(self).to(t2).with(_, 23));
    expect((put_atom, int), from(self).to(t3).with(_, 23));
  }
  { // Subtest.
    MESSAGE("When an actor sends 'leave' to the group's intermediary.");
    MESSAGE("And an actors sends to the group's intermediary.");
    inject((leave_atom, strong_actor_ptr),
           from(self)
             .to(intermediary)
             .with(leave_atom_v, actor_cast<strong_actor_ptr>(t3)));
    self->send(grp, put_atom_v, 37337);
    MESSAGE("Then only the two remaining subscribers receive the message.");
    self->send(grp, put_atom_v, 37337);
    expect((put_atom, int), from(self).to(t1).with(_, 37337));
    expect((put_atom, int), from(self).to(t2).with(_, 37337));
    disallow((put_atom, int), from(self).to(t3).with(_, 37337));
  }
}

END_FIXTURE_SCOPE()
