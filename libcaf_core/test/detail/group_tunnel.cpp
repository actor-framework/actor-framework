/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE detail.group_tunnel

#include "caf/detail/group_tunnel.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

namespace {

class mock_module : public group_module {
public:
  using super = group_module;

  explicit mock_module(actor_system& sys) : super(sys, "mock") {
    // nop
  }

  void stop() {
    for (auto& kvp : instances)
      kvp.second->stop();
    instances.clear();
  }

  detail::group_tunnel_ptr get_impl(const std::string& group_name) {
    if (auto i = instances.find(group_name); i != instances.end()) {
      return i->second;
    } else {
      auto wrapped = system().groups().get_local(group_name);
      auto result = make_counted<detail::group_tunnel>(
        this, group_name, wrapped.get()->intermediary());
      instances.emplace(group_name, result);
      return result;
    }
  }

  expected<group> get(const std::string& group_name) {
    auto result = get_impl(group_name);
    return group{result.get()};
  }

  std::map<std::string, detail::group_tunnel_ptr> instances;
};

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
    uut = make_counted<mock_module>(sys);
    origin = sys.groups().get_local("test");
    intermediary = origin.get()->intermediary();
    tunnel = uut->get_impl("test");
    proxy = group{tunnel.get()};
    worker = tunnel->worker();
    run();
  }

  ~fixture() {
    // Groups keep their subscribers alive (on purpose). Since we don't want to
    // manually kill all our testee actors, we simply force the group modules to
    // stop here.
    for (auto& kvp : uut->instances)
      kvp.second->stop();
    sys.groups().get_module("local")->stop();
  }

  intrusive_ptr<mock_module> uut;
  group origin;
  actor intermediary;
  detail::group_tunnel_ptr tunnel;
  group proxy;
  actor worker;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(group_tunnel_tests, fixture)

CAF_TEST(tunnels automatically subscribe to their origin on first subscribe) {
  CAF_MESSAGE("Given a group with two subscribers and a tunnel.");
  sys.spawn_in_group<lazy_init>(origin, testee_impl);
  sys.spawn_in_group<lazy_init>(origin, testee_impl);
  { // Subtest.
    CAF_MESSAGE("When an actors joins the tunnel.");
    sys.spawn_in_group<lazy_init>(proxy, testee_impl);
    CAF_MESSAGE("Then the tunnel worker joins the origin group.");
    expect((sys_atom, join_atom), to(worker));
    expect((join_atom, strong_actor_ptr),
           from(worker).to(intermediary).with(_, worker));
    CAF_CHECK(!sched.has_job());
  }
  { // Subtest.
    CAF_MESSAGE("When a second actor joins the tunnel.");
    sys.spawn_in_group<lazy_init>(proxy, testee_impl);
    CAF_MESSAGE("Then no messaging occurs.");
    CAF_CHECK(!sched.has_job());
  }
}

CAF_TEST("tunnels dispatch published messages") {
  CAF_MESSAGE("Given a group with two local subscribers locally and tunneled.");
  auto t1 = sys.spawn_in_group<lazy_init>(origin, testee_impl);
  auto t2 = sys.spawn_in_group<lazy_init>(origin, testee_impl);
  auto t3 = sys.spawn_in_group<lazy_init>(proxy, testee_impl);
  auto t4 = sys.spawn_in_group<lazy_init>(proxy, testee_impl);
  run();
  { // Subtest.
    CAF_MESSAGE("When an actors sends to the group.");
    self->send(origin, put_atom_v, 42);
    CAF_MESSAGE("Then tunnel subscribers receive the forwarded message.");
    expect((put_atom, int), from(self).to(t1).with(_, 42));
    expect((put_atom, int), from(self).to(t2).with(_, 42));
    expect((put_atom, int), from(self).to(worker).with(_, 42));
    expect((put_atom, int), from(self).to(t3).with(_, 42));
    expect((put_atom, int), from(self).to(t4).with(_, 42));
    CAF_CHECK(!sched.has_job());
  }
  { // Subtest.
    CAF_MESSAGE("When an actors sends to the tunnel.");
    self->send(proxy, put_atom_v, 42);
    CAF_MESSAGE("Then the message travels to the origin.");
    CAF_MESSAGE("And tunnel subscribers get the forwarded message eventually.");
    expect((sys_atom, forward_atom, message), from(self).to(worker));
    expect((forward_atom, message), from(self).to(intermediary));
    expect((put_atom, int), from(self).to(t1).with(_, 42));
    expect((put_atom, int), from(self).to(t2).with(_, 42));
    expect((put_atom, int), from(self).to(worker).with(_, 42));
    expect((put_atom, int), from(self).to(t3).with(_, 42));
    expect((put_atom, int), from(self).to(t4).with(_, 42));
    CAF_CHECK(!sched.has_job());
  }
}

CAF_TEST(tunnels automatically unsubscribe from their origin) {
  CAF_MESSAGE("Given a group with two local subscribers locally and tunneled.");
  auto t1 = sys.spawn_in_group<lazy_init>(origin, testee_impl);
  auto t2 = sys.spawn_in_group<lazy_init>(origin, testee_impl);
  auto t3 = sys.spawn_in_group<lazy_init>(proxy, testee_impl);
  auto t4 = sys.spawn_in_group<lazy_init>(proxy, testee_impl);
  run();
  { // Subtest.
    CAF_MESSAGE("When the first actor leaves the tunnel.");
    proxy.unsubscribe(actor_cast<actor_control_block*>(t3));
    CAF_MESSAGE("Then no messaging occurs.");
    CAF_CHECK(!sched.has_job());
  }
  { // Subtest.
    CAF_MESSAGE("When an actors sends to the group after the unsubscribe.");
    self->send(origin, put_atom_v, 42);
    CAF_MESSAGE("Then the unsubscribed actor no longer receives the message.");
    expect((put_atom, int), from(self).to(t1).with(_, 42));
    expect((put_atom, int), from(self).to(t2).with(_, 42));
    expect((put_atom, int), from(self).to(worker).with(_, 42));
    disallow((put_atom, int), from(self).to(t3).with(_, 42));
    expect((put_atom, int), from(self).to(t4).with(_, 42));
    CAF_CHECK(!sched.has_job());
  }
  { // Subtest.
    CAF_MESSAGE("When the second actor also unsubscribes from the tunnel.");
    proxy.unsubscribe(actor_cast<actor_control_block*>(t4));
    CAF_MESSAGE("Then the tunnel unsubscribes from its origin.");
    expect((sys_atom, leave_atom), to(worker));
    expect((leave_atom, strong_actor_ptr),
           from(worker).to(intermediary).with(_, worker));
  }
  { // Subtest.
    CAF_MESSAGE("When an actors sends to the group after the tunnel left.");
    self->send(origin, put_atom_v, 42);
    CAF_MESSAGE("Then no message arrives at the tunnel.");
    expect((put_atom, int), from(self).to(t1).with(_, 42));
    expect((put_atom, int), from(self).to(t2).with(_, 42));
    disallow((put_atom, int), from(self).to(worker).with(_, 42));
    CAF_CHECK(!sched.has_job());
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
