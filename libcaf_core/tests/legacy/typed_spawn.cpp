// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE typed_spawn

#include "caf/anon_mail.hpp"
#include "caf/mail_cache.hpp"
#include "caf/string_algorithms.hpp"

#include "core-test.hpp"

#define ERROR_HANDLER [&](error& err) { CAF_FAIL(err); }

using std::string;

using namespace caf;
using namespace std::string_literals;

namespace {

// check invariants of type system
using dummy1 = typed_actor<result<void>(int, int), result<double>(double)>;

using dummy2 = dummy1::extend<result<void>(ok_atom)>;

static_assert(std::is_convertible_v<dummy2, dummy1>,
              "handle not assignable to narrower definition");

using dummy3 = typed_actor<result<void>(float, int)>;
using dummy4 = typed_actor<result<double>(int)>;
using dummy5 = dummy4::extend_with<dummy3>;

static_assert(std::is_convertible_v<dummy5, dummy3>,
              "handle not assignable to narrower definition");

static_assert(std::is_convertible_v<dummy5, dummy4>,
              "handle not assignable to narrower definition");

// -- simple request/response test ---------------------------------------------

using server_type = typed_actor<result<bool>(my_request)>;

server_type::behavior_type typed_server1() {
  return {
    [](const my_request& req) { return req.a == req.b; },
  };
}

server_type::behavior_type typed_server2(server_type::pointer) {
  return typed_server1();
}

class typed_server3 : public server_type::base {
public:
  typed_server3(actor_config& cfg, const string& line, actor buddy)
    : server_type::base(cfg) {
    anon_mail(line).send(buddy);
  }

  behavior_type make_behavior() override {
    return typed_server2(this);
  }
};

void client(event_based_actor* self, const actor& parent,
            const server_type& serv) {
  self->mail(my_request{0, 0}).request(serv, infinite).then([=](bool val1) {
    CHECK_EQ(val1, true);
    self->mail(my_request{10, 20}).request(serv, infinite).then([=](bool val2) {
      CHECK_EQ(val2, false);
      self->mail(ok_atom_v).send(parent);
    });
  });
}

// -- test skipping of messages intentionally + using become() -----------------

using event_testee_type
  = typed_actor<result<string>(get_state_atom), result<void>(string),
                result<void>(float), result<int>(int)>;

class event_testee : public event_testee_type::base {
public:
  event_testee(actor_config& cfg)
    : event_testee_type::base(cfg), cache(this, 10) {
    // nop
  }

  behavior_type wait4string() {
    return {
      partial_behavior_init,
      [](get_state_atom) { return "wait4string"; },
      [this](const string&) {
        become(wait4int());
        cache.unstash();
      },
      [this](message msg) { cache.stash(std::move(msg)); },
    };
  }

  behavior_type wait4int() {
    return {
      partial_behavior_init,
      [](get_state_atom) { return "wait4int"; },
      [this](int) -> int {
        become(wait4float());
        cache.unstash();
        return 42;
      },
      [this](message msg) { cache.stash(std::move(msg)); },
    };
  }

  behavior_type wait4float() {
    return {
      partial_behavior_init,
      [](get_state_atom) { return "wait4float"; },
      [this](float) {
        become(wait4string());
        cache.unstash();
      },
      [this](message msg) { cache.stash(std::move(msg)); },
    };
  }

  behavior_type make_behavior() override {
    return wait4int();
  }

  mail_cache cache;
};

// -- simple 'forwarding' chain ------------------------------------------------

using string_actor = typed_actor<result<string>(string)>;

string_actor::behavior_type string_reverter() {
  return {
    [](string& str) -> string {
      std::reverse(str.begin(), str.end());
      return std::move(str);
    },
  };
}

// uses `return delegate(...)`
string_actor::behavior_type string_delegator(string_actor::pointer self,
                                             string_actor master, bool leaf) {
  auto next = leaf ? self->spawn(string_delegator, master, false) : master;
  self->link_to(next);
  return {
    [=](string& str) -> delegated<string> {
      return self->delegate(next, std::move(str));
    },
  };
}

using maybe_string_actor = typed_actor<result<ok_atom, string>(string)>;

maybe_string_actor::behavior_type maybe_string_reverter() {
  return {
    [](string& str) -> result<ok_atom, string> {
      if (str.empty())
        return sec::invalid_argument;
      std::reverse(str.begin(), str.end());
      return {ok_atom_v, std::move(str)};
    },
  };
}

maybe_string_actor::behavior_type
maybe_string_delegator(maybe_string_actor::pointer self,
                       const maybe_string_actor& x) {
  self->link_to(x);
  return {
    [=](string& s) -> delegated<ok_atom, string> {
      return self->delegate(x, std::move(s));
    },
  };
}

// -- sending typed actor handles ----------------------------------------------

int_actor::behavior_type int_fun() {
  return {
    [](int i) { return i * i; },
  };
}

behavior foo(event_based_actor* self) {
  return {
    [=](int i, int_actor server) {
      self->delegate(server, i);
      self->quit();
    },
  };
}

int_actor::behavior_type int_fun2(int_actor::pointer self) {
  return {
    [=](int i) {
      self->monitor(self->current_sender(), [self](const error& reason) {
        CHECK_EQ(reason, exit_reason::normal);
        self->quit();
      });
      return i * i;
    },
  };
}

behavior foo2(event_based_actor* self) {
  return {
    [=](int i, int_actor server) {
      self->delegate(server, i);
      self->quit();
    },
  };
}

float_actor::behavior_type float_fun(float_actor::pointer self) {
  return {
    [=](float a) {
      CHECK_EQ(a, 1.0f);
      self->quit(exit_reason::user_shutdown);
    },
  };
}

int_actor::behavior_type foo3(int_actor::pointer self) {
  auto b = self->spawn<linked>(float_fun);
  self->mail(1.0f).send(b);
  return {
    [=](int) { return 0; },
  };
}

struct fixture : test_coordinator_fixture<> {
  void test_typed_spawn(server_type ts) {
    MESSAGE("the server returns false for inequal numbers");
    inject((my_request), from(self).to(ts).with(my_request{1, 2}));
    expect((bool), from(ts).to(self).with(false));
    MESSAGE("the server returns true for equal numbers");
    inject((my_request), from(self).to(ts).with(my_request{42, 42}));
    expect((bool), from(ts).to(self).with(true));
    CHECK_EQ(sys.registry().running(), 2u);
    auto c1 = self->spawn(client, self, ts);
    run();
    expect((ok_atom), from(c1).to(self).with(ok_atom_v));
    CHECK_EQ(sys.registry().running(), 2u);
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

// -- putting it all together --------------------------------------------------

CAF_TEST(typed_spawns) {
  MESSAGE("run test series with typed_server1");
  test_typed_spawn(sys.spawn(typed_server1));
  self->await_all_other_actors_done();
  MESSAGE("finished test series with `typed_server1`");
  MESSAGE("run test series with typed_server2");
  test_typed_spawn(sys.spawn(typed_server2));
  self->await_all_other_actors_done();
  MESSAGE("finished test series with `typed_server2`");
  auto serv3 = self->spawn<typed_server3>("hi there", self);
  run();
  expect((string), from(serv3).to(self).with("hi there"s));
  test_typed_spawn(serv3);
}

CAF_TEST(event_testee_series) {
  auto et = self->spawn<event_testee>();
  MESSAGE("et->message_types() returns an interface description");
  typed_actor<result<string>(get_state_atom)> sub_et = et;
  std::set<string> iface{"(get_state_atom) -> (std::string)",
                         "(std::string) -> (void)", "(float) -> (void)",
                         "(int32_t) -> (int32_t)"};
  CHECK_EQ(join(sub_et->message_types(), ","), join(iface, ","));
  MESSAGE("the testee skips messages to drive its internal state machine");
  self->mail(1).send(et);
  self->mail(2).send(et);
  self->mail(3).send(et);
  self->mail(.1f).send(et);
  self->mail("hello event testee!"s).send(et);
  self->mail(.2f).send(et);
  self->mail(.3f).send(et);
  self->mail("hello again event testee!"s).send(et);
  self->mail("goodbye event testee!"s).send(et);
  run();
  expect((int), from(et).to(self).with(42));
  expect((int), from(et).to(self).with(42));
  expect((int), from(et).to(self).with(42));
  inject((get_state_atom), from(self).to(sub_et).with(get_state_atom_v));
  expect((string), from(et).to(self).with("wait4int"s));
}

CAF_TEST(string_delegator_chain) {
  // run test series with string reverter
  auto aut = self->spawn(string_delegator, sys.spawn(string_reverter), true);
  std::set<string> iface{"(std::string) -> (std::string)"};
  CHECK_EQ(aut->message_types(), iface);
  inject((string), from(self).to(aut).with("Hello World!"s));
  run();
  expect((string), to(self).with("!dlroW olleH"s));
}

CAF_TEST(maybe_string_delegator_chain) {
  auto lg = log::core::trace("self = {}", self);
  auto aut = sys.spawn(maybe_string_delegator,
                       sys.spawn(maybe_string_reverter));
  MESSAGE("send empty string, expect error");
  inject((string), from(self).to(aut).with(""s));
  run();
  expect((error), to(self).with(sec::invalid_argument));
  MESSAGE("send abcd string, expect dcba");
  inject((string), from(self).to(aut).with("abcd"s));
  run();
  expect((ok_atom, string), to(self).with(ok_atom_v, "dcba"s));
}

CAF_TEST(sending_typed_actors) {
  auto aut = sys.spawn(int_fun);
  self->mail(10, aut).send(self->spawn(foo));
  run();
  expect((int), to(self).with(100));
  self->spawn(foo3);
  run();
}

CAF_TEST(sending_typed_actors_and_down_msg) {
  auto aut = sys.spawn(int_fun2);
  self->mail(10, aut).send(self->spawn(foo2));
  run();
  expect((int), to(self).with(100));
}

CAF_TEST(check_signature) {
  using foo_type = typed_actor<result<ok_atom>(put_atom)>;
  using foo_result_type = result<ok_atom>;
  using bar_type = typed_actor<result<void>(ok_atom)>;
  auto foo_action = [](foo_type::pointer ptr) -> foo_type::behavior_type {
    return {
      [=](put_atom) -> foo_result_type {
        ptr->quit();
        return {ok_atom_v};
      },
    };
  };
  auto bar_action = [=](bar_type::pointer ptr) -> bar_type::behavior_type {
    auto foo = ptr->spawn<linked>(foo_action);
    ptr->mail(put_atom_v).send(foo);
    return {
      [=](ok_atom) { ptr->quit(); },
    };
  };
  auto x = self->spawn(bar_action);
  run();
}

SCENARIO("state classes may use typed pointers") {
  GIVEN("a state class for a statically typed actor type") {
    using foo_type = typed_actor<result<int32_t>(get_atom)>;
    struct foo_state {
      foo_state(foo_type::pointer_view selfptr) : self(selfptr) {
        foo_type hdl{self};
        CHECK_EQ(selfptr, actor_cast<abstract_actor*>(hdl));
        foo_type hdl2{selfptr};
        CHECK_EQ(hdl, hdl2);
      }
      foo_type::behavior_type make_behavior() {
        return {
          [](get_atom) { return int32_t{42}; },
        };
      }
      foo_type::pointer_view self;
    };
    using foo_impl = stateful_actor<foo_state, foo_type::impl>;
    WHEN("spawning a stateful actor using the state class") {
      auto foo = sys.spawn<foo_impl>();
      THEN("the actor calls make_behavior of the state class") {
        inject((get_atom), from(self).to(foo).with(get_atom_v));
        expect((int32_t), from(foo).to(self).with(42));
      }
    }
  }
}

END_FIXTURE_SCOPE()
