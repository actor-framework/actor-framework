// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_registry.hpp"
#include "caf/anon_mail.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_actor_pointer.hpp"
#include "caf/typed_event_based_actor.hpp"

#define ERROR_HANDLER [&](error& err) { fail(err); }

using std::string;

using namespace caf;
using namespace std::string_literals;

namespace {

struct my_request;

using int_actor = caf::typed_actor<caf::result<int32_t>(int32_t)>;
using float_actor = caf::typed_actor<caf::result<void>(float)>;

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(typed_spawn_test, caf::first_custom_type_id + 120)

  CAF_ADD_TYPE_ID(typed_spawn_test, (my_request))
  CAF_ADD_TYPE_ID(typed_spawn_test, (int_actor))
  CAF_ADD_TYPE_ID(typed_spawn_test, (float_actor))
  CAF_ADD_ATOM(typed_spawn_test, get_state_atom)

CAF_END_TYPE_ID_BLOCK(typed_spawn_test)

namespace {

struct my_request {
  int32_t a = 0;
  int32_t b = 0;
  my_request() = default;
  my_request(int a, int b) : a(a), b(b) {
    // nop
  }
};

inline bool operator==(const my_request& x, const my_request& y) {
  return std::tie(x.a, x.b) == std::tie(y.a, y.b);
}

template <class Inspector>
bool inspect(Inspector& f, my_request& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b));
}

template <class T>
void check_received(scoped_actor& self, const T& rhs) {
  auto& this_test = test::runnable::current();
  auto received_msg = std::make_shared<bool>(false);
  self->receive([&this_test, &received_msg, &rhs](const T& message) {
    *received_msg = true;
    this_test.check_eq(message, rhs);
  });
  this_test.check(*received_msg);
}

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
  self->mail(my_request{0, 0})
    .request(serv, infinite)
    .then([self, parent, serv](bool val1) {
      test::runnable::current().check_eq(val1, true);
      self->mail(my_request{10, 20})
        .request(serv, infinite)
        .then([self, parent](bool val2) {
          test::runnable::current().check_eq(val2, false);
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
  event_testee(actor_config& cfg) : event_testee_type::base(cfg) {
    set_default_handler(skip);
  }

  behavior_type wait4string() {
    return {
      partial_behavior_init,
      [](get_state_atom) { return "wait4string"; },
      [this](const string&) { become(wait4int()); },
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
  self->mail(1.0f).send(b);
  return {
    [=](int) { return 0; },
  };
}

struct fixture : test::fixture::deterministic {
  scoped_actor self{sys};
  void test_typed_spawn(server_type ts) {
    log::test::debug("the server returns false for inequal numbers");
    inject().with(my_request{1, 2}).from(self).to(ts);
    auto& this_test = test::runnable::current();
    check_received(self, false);
    log::test::debug("the server returns true for equal numbers");
    inject().with(my_request{42, 42}).from(self).to(ts);
    check_received(self, true);
    this_test.check_eq(sys.registry().running(), 2u);
    auto c1 = self->spawn(client, self, ts);
    dispatch_messages();
    auto received_msg = std::make_shared<bool>(false);
    self->receive([&received_msg](ok_atom) { *received_msg = true; });
    this_test.check(*received_msg);
    this_test.check_eq(sys.registry().running(), 2u);
  }
};

WITH_FIXTURE(fixture) {

// -- putting it all together --------------------------------------------------

TEST("typed_spawns") {
  log::test::debug("run test series with typed_server1");
  test_typed_spawn(sys.spawn(typed_server1));
  self->await_all_other_actors_done();
  log::test::debug("finished test series with `typed_server1`");
  log::test::debug("run test series with typed_server2");
  test_typed_spawn(sys.spawn(typed_server2));
  self->await_all_other_actors_done();
  log::test::debug("finished test series with `typed_server2`");
  auto serv3 = self->spawn<typed_server3>("hi there", self);
  dispatch_messages();
  check_received(self, "hi there"s);
  test_typed_spawn(serv3);
}

TEST("event_testee_series") {
  auto et = self->spawn<event_testee>();
  log::test::debug("et->message_types() returns an interface description");
  typed_actor<result<string>(get_state_atom)> sub_et = et;
  std::set<string> iface{"(get_state_atom) -> (std::string)",
                         "(std::string) -> (void)", "(float) -> (void)",
                         "(int32_t) -> (int32_t)"};
  check_eq(join(sub_et->message_types(), ","), join(iface, ","));
  log::test::debug(
    "the testee skips messages to drive its internal state machine");
  self->mail(1).send(et);
  self->mail(2).send(et);
  self->mail(3).send(et);
  self->mail(.1f).send(et);
  self->mail("hello event testee!"s).send(et);
  self->mail(.2f).send(et);
  self->mail(.3f).send(et);
  self->mail("hello again event testee!"s).send(et);
  self->mail("goodbye event testee!"s).send(et);
  dispatch_messages();
  check_received(self, 42);
  check_received(self, 42);
  check_received(self, 42);
  inject().with(get_state_atom_v).from(self).to(sub_et);
  check_received(self, "wait4int"s);
}

TEST("string_delegator_chain") {
  // run test series with string reverter
  auto aut = self->spawn<monitored>(string_delegator,
                                    sys.spawn(string_reverter), true);
  std::set<string> iface{"(std::string) -> (std::string)"};
  check_eq(aut->message_types(), iface);
  inject().with("Hello World!"s).from(self).to(aut);
  dispatch_messages();
  check_received(self, "!dlroW olleH"s);
}

TEST("maybe_string_delegator_chain") {
  auto lg = log::core::trace("self = {}", self);
  auto aut = sys.spawn(maybe_string_delegator,
                       sys.spawn(maybe_string_reverter));
  log::test::debug("send empty string, expect error");
  inject().with(""s).from(self).to(aut);
  dispatch_messages();
  check_received<error>(self, sec::invalid_argument);
  log::test::debug("send abcd string, expect dcba");
  inject().with("abcd"s).from(self).to(aut);
  dispatch_messages();
  auto& this_test = test::runnable::current();
  auto received_msg = std::make_shared<bool>(false);
  self->receive([&this_test, &received_msg](ok_atom, const string& message) {
    *received_msg = true;
    this_test.check_eq(message, "dcba"s);
  });
  this_test.check(*received_msg);
}

TEST("sending_typed_actors") {
  auto aut = sys.spawn(int_fun);
  self->mail(10, aut).send(self->spawn(foo));
  dispatch_messages();
  check_received(self, 100);
  self->spawn(foo3);
  dispatch_messages();
}

TEST("sending_typed_actors_and_down_msg") {
  auto aut = sys.spawn(int_fun2);
  self->mail(10, aut).send(self->spawn(foo2));
  dispatch_messages();
  check_received(self, 100);
}

TEST("check_signature") {
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
  dispatch_messages();
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
        check_received(self, 42);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)

TEST_INIT() {
  caf::init_global_meta_objects<caf::id_block::typed_spawn_test>();
}

} // namespace
