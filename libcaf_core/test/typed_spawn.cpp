// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/config.hpp"

// exclude this suite; seems to be too much to swallow for MSVC
#ifndef CAF_WINDOWS

#  define CAF_SUITE typed_spawn

#  include "core-test.hpp"

#  include "caf/string_algorithms.hpp"

#  include "caf/all.hpp"

#  define ERROR_HANDLER [&](error& err) { CAF_FAIL(err); }

using std::string;

using namespace caf;
using namespace std::string_literals;

namespace {

static_assert(std::is_same<reacts_to<int, int>, result<void>(int, int)>::value);

static_assert(std::is_same<replies_to<double>::with<double>,
                           result<double>(double)>::value);

// check invariants of type system
using dummy1 = typed_actor<result<void>(int, int), result<double>(double)>;

using dummy2 = dummy1::extend<reacts_to<ok_atom>>;

static_assert(std::is_convertible<dummy2, dummy1>::value,
              "handle not assignable to narrower definition");

using dummy3 = typed_actor<reacts_to<float, int>>;
using dummy4 = typed_actor<replies_to<int>::with<double>>;
using dummy5 = dummy4::extend_with<dummy3>;

static_assert(std::is_convertible<dummy5, dummy3>::value,
              "handle not assignable to narrower definition");

static_assert(std::is_convertible<dummy5, dummy4>::value,
              "handle not assignable to narrower definition");

/******************************************************************************
 *                        simple request/response test                        *
 ******************************************************************************/

using server_type = typed_actor<replies_to<my_request>::with<bool>>;

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
    anon_send(buddy, line);
  }

  behavior_type make_behavior() override {
    return typed_server2(this);
  }
};

void client(event_based_actor* self, const actor& parent,
            const server_type& serv) {
  self->request(serv, infinite, my_request{0, 0}).then([=](bool val1) {
    CAF_CHECK_EQUAL(val1, true);
    self->request(serv, infinite, my_request{10, 20}).then([=](bool val2) {
      CAF_CHECK_EQUAL(val2, false);
      self->send(parent, ok_atom_v);
    });
  });
}

/******************************************************************************
 *          test skipping of messages intentionally + using become()          *
 ******************************************************************************/

using event_testee_type
  = typed_actor<replies_to<get_state_atom>::with<string>,
                replies_to<string>::with<void>, replies_to<float>::with<void>,
                replies_to<int>::with<int>>;

class event_testee : public event_testee_type::base {
public:
  event_testee(actor_config& cfg) : event_testee_type::base(cfg) {
    set_default_handler(skip);
  }

  behavior_type wait4string() {
    return {
      partial_behavior_init,
      [=](get_state_atom) { return "wait4string"; },
      [=](const string&) { become(wait4int()); },
    };
  }

  behavior_type wait4int() {
    return {
      partial_behavior_init,
      [=](get_state_atom) { return "wait4int"; },
      [=](int) -> int {
        become(wait4float());
        return 42;
      },
    };
  }

  behavior_type wait4float() {
    return {
      partial_behavior_init,
      [=](get_state_atom) { return "wait4float"; },
      [=](float) { become(wait4string()); },
    };
  }

  behavior_type make_behavior() override {
    return wait4int();
  }
};

/******************************************************************************
 *                         simple 'forwarding' chain                          *
 ******************************************************************************/

using string_actor = typed_actor<replies_to<string>::with<string>>;

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

using maybe_string_actor
  = typed_actor<replies_to<string>::with<ok_atom, string>>;

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

/******************************************************************************
 *                        sending typed actor handles                         *
 ******************************************************************************/

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
    CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
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
      CAF_CHECK_EQUAL(a, 1.0f);
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

struct fixture : test_coordinator_fixture<> {
  void test_typed_spawn(server_type ts) {
    CAF_MESSAGE("the server returns false for inequal numbers");
    inject((my_request), from(self).to(ts).with(my_request{1, 2}));
    expect((bool), from(ts).to(self).with(false));
    CAF_MESSAGE("the server returns true for equal numbers");
    inject((my_request), from(self).to(ts).with(my_request{42, 42}));
    expect((bool), from(ts).to(self).with(true));
    CAF_CHECK_EQUAL(sys.registry().running(), 2u);
    auto c1 = self->spawn(client, self, ts);
    run();
    expect((ok_atom), from(c1).to(self).with(ok_atom_v));
    CAF_CHECK_EQUAL(sys.registry().running(), 2u);
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(typed_spawn_tests, fixture)

/******************************************************************************
 *                             put it all together                            *
 ******************************************************************************/

CAF_TEST(typed_spawns) {
  CAF_MESSAGE("run test series with typed_server1");
  test_typed_spawn(sys.spawn(typed_server1));
  self->await_all_other_actors_done();
  CAF_MESSAGE("finished test series with `typed_server1`");
  CAF_MESSAGE("run test series with typed_server2");
  test_typed_spawn(sys.spawn(typed_server2));
  self->await_all_other_actors_done();
  CAF_MESSAGE("finished test series with `typed_server2`");
  auto serv3 = self->spawn<typed_server3>("hi there", self);
  run();
  expect((string), from(serv3).to(self).with("hi there"s));
  test_typed_spawn(serv3);
}

CAF_TEST(event_testee_series) {
  auto et = self->spawn<event_testee>();
  CAF_MESSAGE("et->message_types() returns an interface description");
  typed_actor<replies_to<get_state_atom>::with<string>> sub_et = et;
  std::set<string> iface{"(get_state_atom) -> (std::string)",
                         "(std::string) -> (void)", "(float) -> (void)",
                         "(int32_t) -> (int32_t)"};
  CAF_CHECK_EQUAL(join(sub_et->message_types(), ","), join(iface, ","));
  CAF_MESSAGE("the testee skips messages to drive its internal state machine");
  self->send(et, 1);
  self->send(et, 2);
  self->send(et, 3);
  self->send(et, .1f);
  self->send(et, "hello event testee!"s);
  self->send(et, .2f);
  self->send(et, .3f);
  self->send(et, "hello again event testee!"s);
  self->send(et, "goodbye event testee!"s);
  run();
  expect((int), from(et).to(self).with(42));
  expect((int), from(et).to(self).with(42));
  expect((int), from(et).to(self).with(42));
  inject((get_state_atom), from(self).to(sub_et).with(get_state_atom_v));
  expect((string), from(et).to(self).with("wait4int"s));
}

CAF_TEST(string_delegator_chain) {
  // run test series with string reverter
  auto aut = self->spawn<monitored>(string_delegator,
                                    sys.spawn(string_reverter), true);
  std::set<string> iface{"(std::string) -> (std::string)"};
  CAF_CHECK_EQUAL(aut->message_types(), iface);
  inject((string), from(self).to(aut).with("Hello World!"s));
  run();
  expect((string), to(self).with("!dlroW olleH"s));
}

CAF_TEST(maybe_string_delegator_chain) {
  CAF_LOG_TRACE(CAF_ARG(self));
  auto aut = sys.spawn(maybe_string_delegator,
                       sys.spawn(maybe_string_reverter));
  CAF_MESSAGE("send empty string, expect error");
  inject((string), from(self).to(aut).with(""s));
  run();
  expect((error), to(self).with(sec::invalid_argument));
  CAF_MESSAGE("send abcd string, expect dcba");
  inject((string), from(self).to(aut).with("abcd"s));
  run();
  expect((ok_atom, string), to(self).with(ok_atom_v, "dcba"s));
}

CAF_TEST(sending_typed_actors) {
  auto aut = sys.spawn(int_fun);
  self->send(self->spawn(foo), 10, aut);
  run();
  expect((int), to(self).with(100));
  self->spawn(foo3);
  run();
}

CAF_TEST(sending_typed_actors_and_down_msg) {
  auto aut = sys.spawn(int_fun2);
  self->send(self->spawn(foo2), 10, aut);
  run();
  expect((int), to(self).with(100));
}

CAF_TEST(check_signature) {
  using foo_type = typed_actor<replies_to<put_atom>::with<ok_atom>>;
  using foo_result_type = result<ok_atom>;
  using bar_type = typed_actor<reacts_to<ok_atom>>;
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
  auto x = self->spawn(bar_action);
  run();
}

CAF_TEST_FIXTURE_SCOPE_END()

#endif // CAF_WINDOWS
