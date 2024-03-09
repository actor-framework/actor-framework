// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/typed_event_based_actor.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_actor_pointer.hpp"

using namespace caf;
using namespace std::literals;

// -- type IDs -----------------------------------------------------------------

struct my_request {
  my_request() = default;
  my_request(int a, int b) : a(a), b(b) {
  }
  int32_t a = 0;
  int32_t b = 0;
  bool operator==(const my_request& other) const {
    return a == other.a && b == other.b;
  }
};

template <class Inspector>
bool inspect(Inspector& f, my_request& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

struct int_trait {
  using signatures = type_list<caf::result<int32_t>(int32_t)>;
};
using int_actor = caf::typed_actor<int_trait>;

struct float_trait {
  using signatures = type_list<caf::result<void>(float)>;
};
using float_actor = caf::typed_actor<float_trait>;

struct server_trait {
  using signatures = type_list<result<bool>(my_request)>;
};
using server_actor = caf::typed_actor<server_trait>;

#define ADD_TYPE_ID(type) CAF_ADD_TYPE_ID(typed_event_based_actor_test, type)

CAF_BEGIN_TYPE_ID_BLOCK(typed_event_based_actor_test,
                        caf::first_custom_type_id + 110)
  ADD_TYPE_ID((my_request))
  ADD_TYPE_ID((int_actor))
  ADD_TYPE_ID((float_actor))
  ADD_TYPE_ID((server_actor))
  CAF_ADD_ATOM(typed_event_based_actor_test, get_state_atom)
CAF_END_TYPE_ID_BLOCK(typed_event_based_actor_test)

#undef ADD_TYPE_ID

namespace {

// -- simple request/response test ---------------------------------------------

server_actor::behavior_type typed_server1() {
  return {
    [](const my_request& req) { return req.a == req.b; },
  };
}

server_actor::behavior_type typed_server2(server_actor::pointer) {
  return typed_server1();
}

class typed_server3 : public server_actor::base {
public:
  typed_server3(actor_config& cfg, const std::string& line, actor buddy)
    : server_actor::base(cfg) {
    anon_send(buddy, line);
  }

  behavior_type make_behavior() override {
    return typed_server2(this);
  }
};

void client(event_based_actor* self, const actor& parent,
            const server_actor& serv) {
  self->request(serv, infinite, my_request{0, 0}).then([=](bool val1) {
    test::runnable::current().check_eq(val1, true);
    self->request(serv, infinite, my_request{10, 20}).then([=](bool val2) {
      test::runnable::current().check_eq(val2, false);
      self->send(parent, ok_atom_v);
    });
  });
}

// -- test skipping of messages intentionally + using become() -----------------

struct testee_trait {
  using signatures
    = type_list<result<std::string>(get_state_atom), result<void>(std::string),
                result<void>(float), result<int>(int)>;
};

using event_testee_type = typed_actor<testee_trait>;

class event_testee : public event_testee_type::base {
public:
  event_testee(actor_config& cfg) : event_testee_type::base(cfg) {
    set_default_handler(skip);
  }

  behavior_type wait4string() {
    return {
      partial_behavior_init,
      [](get_state_atom) { return "wait4string"; },
      [this](const std::string&) { become(wait4int()); },
    };
  }

  behavior_type wait4int() {
    return {
      partial_behavior_init,
      [](get_state_atom) { return "wait4int"; },
      [this](int) -> int {
        become(wait4float());
        return 42;
      },
    };
  }

  behavior_type wait4float() {
    return {
      partial_behavior_init,
      [](get_state_atom) { return "wait4float"; },
      [this](float) { become(wait4string()); },
    };
  }

  behavior_type make_behavior() override {
    return wait4int();
  }
};

// -- simple 'forwarding' chain ------------------------------------------------

struct string_trait {
  using signatures = type_list<result<std::string>(std::string)>;
};

using string_actor = typed_actor<string_trait>;

string_actor::behavior_type string_reverter() {
  return {
    [](std::string& str) -> std::string {
      std::reverse(str.begin(), str.end());
      return std::move(str);
    },
  };
}

// uses `return delegate(...)`
string_actor::behavior_type string_delegator(string_actor::pointer self,
                                             string_actor next) {
  self->link_to(next);
  return {
    [=](std::string& str) -> delegated<std::string> {
      return self->delegate(next, std::move(str));
    },
  };
}

struct maybe_string_trait {
  using signatures = type_list<result<ok_atom, std::string>(std::string)>;
};

using maybe_string_actor = typed_actor<maybe_string_trait>;

maybe_string_actor::behavior_type maybe_string_reverter() {
  return {
    [](std::string& str) -> result<ok_atom, std::string> {
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
    [=](std::string& s) -> delegated<ok_atom, std::string> {
      return self->delegate(x, std::move(s));
    },
  };
}

struct typed_fixture : test::fixture::deterministic {
  typed_fixture() {
    self = sys.spawn(
      [](event_based_actor*) -> behavior { return [](message) {}; });
  }
  actor self;
};

WITH_FIXTURE(typed_fixture) {

TEST("spawning a typed actor and sending messages") {
  auto test_typed_spawn = [this](server_actor ts) {
    log::test::debug("the server returns false for inequal numbers");
    inject().with(my_request{1, 2}).from(self).to(ts);
    expect<bool>().with(false).from(ts).to(self);
    log::test::debug("the server returns true for equal numbers");
    inject().with(my_request{42, 42}).from(self).to(ts);
    expect<bool>().with(true).from(ts).to(self);
    log::test::debug("client and server communicate using request/then");
    check_eq(sys.registry().running(), 2u);
    auto c1 = sys.spawn(client, self, ts);
    dispatch_message();
    dispatch_message();
    dispatch_message();
    dispatch_message();
    expect<ok_atom>().with(ok_atom_v).from(c1).to(self);
    check_eq(sys.registry().running(), 2u);
  };
  SECTION("run test series with typed_server1") {
    test_typed_spawn(sys.spawn(typed_server1));
    sys.registry().await_running_count_equal(1);
  }
  SECTION("run test series with typed_server2") {
    test_typed_spawn(sys.spawn(typed_server2));
    sys.registry().await_running_count_equal(1);
  }
  SECTION("run test series with typed_server3") {
    auto serv3 = sys.spawn<typed_server3>("hi there", self);
    expect<std::string>().with("hi there"s).to(self);
    test_typed_spawn(serv3);
  }
}

TEST("chainging the behavior at runtime and skipping messages") {
  auto et = sys.spawn<event_testee>();
  typed_actor<result<std::string>(get_state_atom)> sub_et = et;
  SECTION("et->message_types() returns an interface description") {
    std::set<std::string> iface{"(get_state_atom) -> (std::string)",
                                "(std::string) -> (void)", "(float) -> (void)",
                                "(int32_t) -> (int32_t)"};
    check_eq(join(sub_et->message_types(), ","), join(iface, ","));
  }
  SECTION("the testee skips messages to drive its internal state machine") {
    scoped_actor sf{sys};
    auto receive_or_fail = [&, this]() {
      sf->receive([this](int a) { check_eq(a, 42); },
                  [this](message) { fail("Unexpected message"); },
                  caf::after(10ms) >> [this]() { fail("Timeout"); });
    };
    sf->send(et, 1);
    sf->send(et, 2);
    sf->send(et, 3);
    sf->send(et, .1f);
    dispatch_messages();
    receive_or_fail();
    sf->send(et, "hello event testee!"s);
    sf->send(et, .2f);
    dispatch_messages();
    receive_or_fail();
    sf->send(et, .3f);
    sf->send(et, "hello again event testee!"s);
    dispatch_messages();
    receive_or_fail();
    sf->send(et, "goodbye event testee!"s);
    dispatch_message();
    inject().with(get_state_atom_v).from(self).to(sub_et);
    expect<std::string>().with("wait4int"s).from(et).to(self);
  }
}

TEST("starting a string delegator chain") {
  auto reverter = sys.spawn(string_reverter);
  auto delegator = sys.spawn(string_delegator, reverter);
  auto aut = sys.spawn(string_delegator, delegator);
  SECTION("message_types() returns an interface description") {
    std::set<std::string> iface{"(std::string) -> (std::string)"};
    check_eq(aut->message_types(), iface);
  }
  inject().with("Hello World!"s).from(self).to(aut);
  expect<std::string>().from(self).to(delegator);
  expect<std::string>().from(self).to(reverter);
  expect<std::string>().with("!dlroW olleH"s).from(reverter).to(self);
  inject_exit(aut);
}

TEST("starting a failable delegator chain") {
  auto aut = sys.spawn(maybe_string_delegator,
                       sys.spawn(maybe_string_reverter));
  SECTION("send empty string, expect error") {
    inject().with(""s).from(self).to(aut);
    dispatch_message();
    expect<error>().with(sec::invalid_argument).to(self);
  }
  SECTION("send abcd string, expect dcba") {
    inject().with("abcd"s).from(self).to(aut);
    dispatch_message();
    expect<ok_atom, std::string>().with(ok_atom_v, "dcba"s).to(self);
  }
}

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
      test::runnable::current().check_eq(a, 1.0f);
      self->quit(exit_reason::user_shutdown);
    },
  };
}

int_actor::behavior_type foo3(int_actor::pointer self) {
  auto b = self->spawn<linked>(float_fun);
  self->send(b, 1.0f);
  return {
    [=](int) { return 0; },
  };
}

TEST("sending typed actors") {
  auto aut = sys.spawn(int_fun);
  auto f = sys.spawn(foo);
  inject().with(10, aut).from(self).to(f);
  expect<int>().with(10).to(aut);
  expect<int>().with(100).to(self);
  sys.spawn(foo3);
  check_eq(dispatch_messages(), 1u);
}

int_actor::behavior_type int_fun2(int_actor::pointer self) {
  self->set_down_handler([=](down_msg& dm) {
    test::runnable::current().check_eq(dm.reason, exit_reason::normal);
    self->quit();
  });
  return {
    [=](int i) {
      self->monitor(self->current_sender());
      return i * i;
    },
  };
}

TEST("sending typed actors and down msg") {
  auto aut = sys.spawn(int_fun2);
  auto buddy = sys.spawn(foo2);
  inject().with(10, aut).from(self).to(buddy);
  expect<int>().with(10).to(aut);
  expect<int>().with(100).to(self);
}

TEST("check signature") {
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
    ptr->send(foo, put_atom_v);
    return {
      [=](ok_atom) { ptr->quit(); },
    };
  };
  auto x = sys.spawn(bar_action);
  check_eq(dispatch_messages(), 1u);
}

SCENARIO("state classes may use typed pointers") {
  GIVEN("a state class for a statically typed actor type") {
    using foo_type = typed_actor<result<int32_t>(get_atom)>;
    struct foo_state {
      foo_state(foo_type::pointer_view selfptr) : self(selfptr) {
        foo_type hdl{self};
        test::runnable::current().check_eq(selfptr,
                                           actor_cast<abstract_actor*>(hdl));
        foo_type hdl2{selfptr};
        test::runnable::current().check_eq(hdl, hdl2);
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
        inject().with(get_atom_v).from(self).to(foo);
        expect<int32_t>().with(42).from(foo).to(self);
      }
    }
  }
}

} // WITH_FIXTURE(typed_fixture)

TEST_INIT() {
  init_global_meta_objects<id_block::typed_event_based_actor_test>();
}

} // namespace
