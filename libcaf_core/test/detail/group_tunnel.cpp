// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.group_tunnel

#include "caf/detail/group_tunnel.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

class mock_module : public group_module {
public:
  using super = group_module;

  explicit mock_module(actor_system& sys) : super(sys, "mock") {
    // nop
  }

  void stop() override {
    drop_instances();
  }

  expected<group> get(const std::string& group_name) override {
    auto result = get_impl(group_name);
    return group{result.get()};
  }

  detail::group_tunnel_ptr get_unconnected(const std::string& group_name,
                                           const node_id& origin) {
    if (auto i = instances.find(group_name); i != instances.end()) {
      return i->second;
    } else {
      auto result = make_counted<detail::group_tunnel>(this, group_name,
                                                       origin);
      instances.emplace(group_name, result);
      return result;
    }
  }

  void drop_instances() {
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

  std::map<std::string, detail::group_tunnel_ptr> instances;
};

struct testee_state {
  int x = 0;
  static inline const char* name = "testee";
};

behavior testee_impl(stateful_actor<testee_state>* self) {
  return {
    [self](put_atom, int x) { self->state.x = x; },
    [self](get_atom) { return self->state.x; },
    [self](const group_down_msg&) { self->quit(); },
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
    uut->stop();
    for (auto& kvp : uut->instances)
      kvp.second->stop();
    sys.groups().get_module("local")->stop();
    run();
  }

  void make_unconnected() {
    uut->drop_instances();
    tunnel = uut->get_unconnected("test", intermediary.node());
    proxy = group{tunnel.get()};
    worker = actor{};
    run();
  }

  void connect_proxy() {
    tunnel->connect(intermediary);
    worker = tunnel->worker();
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

SCENARIO("tunnels automatically subscribe to their origin on first subscribe") {
  GIVEN("a group with two subscribers and a tunnel") {
    sys.spawn_in_group<lazy_init>(origin, testee_impl);
    sys.spawn_in_group<lazy_init>(origin, testee_impl);
    WHEN("an actors joins the tunnel") {
      sys.spawn_in_group<lazy_init>(proxy, testee_impl);
      THEN("the tunnel worker joins the origin group") {
        expect((sys_atom, join_atom), to(worker));
        expect((join_atom, strong_actor_ptr),
               from(worker).to(intermediary).with(_, worker));
        CHECK(!sched.has_job());
      }
    }
    WHEN("a second actor joins the tunnel") {
      sys.spawn_in_group<lazy_init>(proxy, testee_impl);
      THEN("no messaging occurs") {
        CHECK(!sched.has_job());
      }
    }
  }
}

SCENARIO("tunnels dispatch published messages") {
  GIVEN("a group with two local subscribers locally and tunneled") {
    auto t1 = sys.spawn_in_group<lazy_init>(origin, testee_impl);
    auto t2 = sys.spawn_in_group<lazy_init>(origin, testee_impl);
    auto t3 = sys.spawn_in_group<lazy_init>(proxy, testee_impl);
    auto t4 = sys.spawn_in_group<lazy_init>(proxy, testee_impl);
    run();
    WHEN("an actors sends to the group") {
      self->send(origin, put_atom_v, 42);
      THEN("tunnel subscribers receive the forwarded message") {
        expect((put_atom, int), from(self).to(t1).with(_, 42));
        expect((put_atom, int), from(self).to(t2).with(_, 42));
        expect((put_atom, int), from(self).to(worker).with(_, 42));
        expect((put_atom, int), from(self).to(t3).with(_, 42));
        expect((put_atom, int), from(self).to(t4).with(_, 42));
        CHECK(!sched.has_job());
      }
    }
    WHEN("an actors sends to the tunnel") {
      self->send(proxy, put_atom_v, 42);
      THEN("the message travels to the origin and eventually to subscribers") {
        expect((sys_atom, forward_atom, message), from(self).to(worker));
        expect((forward_atom, message), from(self).to(intermediary));
        expect((put_atom, int), from(self).to(t1).with(_, 42));
        expect((put_atom, int), from(self).to(t2).with(_, 42));
        expect((put_atom, int), from(self).to(worker).with(_, 42));
        expect((put_atom, int), from(self).to(t3).with(_, 42));
        expect((put_atom, int), from(self).to(t4).with(_, 42));
        CHECK(!sched.has_job());
      }
    }
  }
}

SCENARIO("tunnels automatically unsubscribe from their origin") {
  GIVEN("a group with two local subscribers locally and tunneled") {
    auto t1 = sys.spawn_in_group<lazy_init>(origin, testee_impl);
    auto t2 = sys.spawn_in_group<lazy_init>(origin, testee_impl);
    auto t3 = sys.spawn_in_group<lazy_init>(proxy, testee_impl);
    auto t4 = sys.spawn_in_group<lazy_init>(proxy, testee_impl);
    run();
    WHEN("the first actor leaves the tunnel") {
      proxy.unsubscribe(actor_cast<actor_control_block*>(t3));
      THEN("no messaging occurs") {
        CHECK(!sched.has_job());
      }
    }
    WHEN("an actors sends to the group after the unsubscribe") {
      self->send(origin, put_atom_v, 42);
      THEN("the unsubscribed actor no longer receives the message") {
        expect((put_atom, int), from(self).to(t1).with(_, 42));
        expect((put_atom, int), from(self).to(t2).with(_, 42));
        expect((put_atom, int), from(self).to(worker).with(_, 42));
        disallow((put_atom, int), from(self).to(t3).with(_, 42));
        expect((put_atom, int), from(self).to(t4).with(_, 42));
        CHECK(!sched.has_job());
      }
    }
    WHEN("the second actor also unsubscribes from the tunnel") {
      proxy.unsubscribe(actor_cast<actor_control_block*>(t4));
      THEN("the tunnel unsubscribes from its origin") {
        expect((sys_atom, leave_atom), to(worker));
        expect((leave_atom, strong_actor_ptr),
               from(worker).to(intermediary).with(_, worker));
      }
    }
    WHEN("an actors sends to the group after the tunnel left") {
      self->send(origin, put_atom_v, 42);
      THEN("no message arrives at the tunnel") {
        expect((put_atom, int), from(self).to(t1).with(_, 42));
        expect((put_atom, int), from(self).to(t2).with(_, 42));
        disallow((put_atom, int), from(self).to(worker).with(_, 42));
        CHECK(!sched.has_job());
      }
    }
  }
}

SCENARIO("tunnels cache messages until connected") {
  GIVEN("an unconnected tunnel with two subscribers") {
    make_unconnected();
    auto t1 = sys.spawn_in_group<lazy_init>(proxy, testee_impl);
    auto t2 = sys.spawn_in_group<lazy_init>(proxy, testee_impl);
    WHEN("an actors sends to the group") {
      self->send(proxy, put_atom_v, 1);
      self->send(proxy, put_atom_v, 2);
      self->send(proxy, put_atom_v, 3);
      THEN("unconnected tunnel caches the messages") {
        CHECK(!sched.has_job());
      }
    }
    WHEN("the tunnel becomes connected") {
      connect_proxy();
      THEN("tunnel subscribes upstream and flushes its cache") {
        expect((sys_atom, join_atom), to(worker));
        expect((sys_atom, forward_atom, message), from(self).to(worker));
        expect((sys_atom, forward_atom, message), from(self).to(worker));
        expect((sys_atom, forward_atom, message), from(self).to(worker));
        expect((join_atom, strong_actor_ptr),
               from(worker).to(intermediary).with(_, worker));
        expect((forward_atom, message), from(self).to(intermediary));
        expect((forward_atom, message), from(self).to(intermediary));
        expect((forward_atom, message), from(self).to(intermediary));
        expect((put_atom, int), from(self).to(worker).with(_, 1));
        expect((put_atom, int), from(self).to(worker).with(_, 2));
        expect((put_atom, int), from(self).to(worker).with(_, 3));
        expect((put_atom, int), from(self).to(t1).with(_, 1));
        expect((put_atom, int), from(self).to(t1).with(_, 2));
        expect((put_atom, int), from(self).to(t1).with(_, 3));
        expect((put_atom, int), from(self).to(t2).with(_, 1));
        expect((put_atom, int), from(self).to(t2).with(_, 2));
        expect((put_atom, int), from(self).to(t2).with(_, 3));
        CHECK(!sched.has_job());
      }
    }
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
