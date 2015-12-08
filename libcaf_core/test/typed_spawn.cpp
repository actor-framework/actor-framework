/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

// exclude this suite; seems to be too much to swallow for MSVC
#ifndef CAF_WINDOWS

#define CAF_SUITE typed_spawn
#include "caf/test/unit_test.hpp"

#include "caf/string_algorithms.hpp"

#include "caf/all.hpp"

using namespace std;
using namespace caf;

using passed_atom = caf::atom_constant<caf::atom("passed")>;

namespace {

enum class mock_errc : uint8_t {
  cannot_revert_empty = 1
};

error make_error(mock_errc x) {
  return {static_cast<uint8_t>(x), atom("mock")};
}

// check invariants of type system
using dummy1 = typed_actor<reacts_to<int, int>,
                           replies_to<double>::with<double>>;

using dummy2 = dummy1::extend<reacts_to<ok_atom>>;

static_assert(std::is_convertible<dummy2, dummy1>::value,
              "handle not assignable to narrower definition");

static_assert(! std::is_convertible<dummy1, dummy2>::value,
              "handle is assignable to broader definition");

/******************************************************************************
 *                        simple request/response test                        *
 ******************************************************************************/

struct my_request {
  int a;
  int b;
};

template <class T>
void serialize(T& in_out, my_request& x, const unsigned int) {
  in_out & x.a;
  in_out & x.b;
}

using server_type = typed_actor<replies_to<my_request>::with<bool>>;

bool operator==(const my_request& lhs, const my_request& rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

server_type::behavior_type typed_server1() {
  return {
    [](const my_request& req) {
      return req.a == req.b;
    }
  };
}

server_type::behavior_type typed_server2(server_type::pointer) {
  return typed_server1();
}

class typed_server3 : public server_type::base {
public:
  typed_server3(actor_config& cfg, const string& line, actor buddy)
      : server_type::base(cfg) {
    send(buddy, line);
  }

  behavior_type make_behavior() override {
    return typed_server2(this);
  }
};

void client(event_based_actor* self, actor parent, server_type serv) {
  self->request(serv, my_request{0, 0}).then(
    [=](bool val1) {
      CAF_CHECK_EQUAL(val1, true);
      self->request(serv, my_request{10, 20}).then(
        [=](bool val2) {
          CAF_CHECK_EQUAL(val2, false);
          self->send(parent, passed_atom::value);
        }
      );
    }
  );
}

void test_typed_spawn(server_type ts) {
  actor_system system;
  scoped_actor self{system};
  self->send(ts, my_request{1, 2});
  self->receive(
    [](bool value) {
      CAF_CHECK_EQUAL(value, false);
    }
  );
  self->send(ts, my_request{42, 42});
  self->receive(
    [](bool value) {
      CAF_CHECK_EQUAL(value, true);
    }
  );
  self->request(ts, my_request{10, 20}).await(
    [](bool value) {
      CAF_CHECK_EQUAL(value, false);
    }
  );
  self->request(ts, my_request{0, 0}).await(
    [](bool value) {
      CAF_CHECK_EQUAL(value, true);
    }
  );
  self->spawn<monitored>(client, self, ts);
  self->receive(
    [](passed_atom) {
      CAF_MESSAGE("received `passed_atom`");
    }
  );
  self->receive(
    [](const down_msg& dmsg) {
      CAF_CHECK_EQUAL(dmsg.reason, exit_reason::normal);
    }
  );
  self->send_exit(ts, exit_reason::user_shutdown);
}

/******************************************************************************
 *          test skipping of messages intentionally + using become()          *
 ******************************************************************************/

struct get_state_msg {};

using event_testee_type = typed_actor<replies_to<get_state_msg>::with<string>,
                                      replies_to<string>::with<void>,
                                      replies_to<float>::with<void>,
                                      replies_to<int>::with<int>>;

class event_testee : public event_testee_type::base {
public:
  event_testee(actor_config& cfg) : event_testee_type::base(cfg) {
    // nop
  }

  behavior_type wait4string() {
    return {on<get_state_msg>() >> [] { return "wait4string"; },
        on<string>() >> [=] { become(wait4int()); },
        (on<float>() || on<int>()) >> skip_message};
  }

  behavior_type wait4int() {
    return {
      on<get_state_msg>() >> [] { return "wait4int"; },
      on<int>() >> [=]()->int {become(wait4float());
        return 42;
      },
      (on<float>() || on<string>()) >> skip_message
    };
  }

  behavior_type wait4float() {
    return {
      on<get_state_msg>() >> [] {
        return "wait4float";
      },
      on<float>() >> [=] { become(wait4string()); },
      (on<string>() || on<int>()) >> skip_message};
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
    [](string& str) {
      std::reverse(str.begin(), str.end());
      return std::move(str);
    }
  };
}

// uses `return request(...).then(...)`
string_actor::behavior_type string_relay(string_actor::pointer self,
                                         string_actor master, bool leaf) {
  auto next = leaf ? self->spawn(string_relay, master, false) : master;
  self->link_to(next);
  return {
    [=](const string& str) {
      return self->request(next, str).then(
        [](string& answer) -> string {
          return std::move(answer);
        }
      );
    }
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
    }
  };
}

using maybe_string_actor = typed_actor<replies_to<string>
                                       ::with<ok_atom, string>>;

maybe_string_actor::behavior_type maybe_string_reverter() {
  return {
    [](string& str) -> maybe<std::tuple<ok_atom, string>> {
      if (str.empty())
        return mock_errc::cannot_revert_empty;
      if (str.empty())
        return none;
      std::reverse(str.begin(), str.end());
      return {ok_atom::value, std::move(str)};
    }
  };
}

maybe_string_actor::behavior_type
maybe_string_delegator(maybe_string_actor::pointer self, maybe_string_actor x) {
  self->link_to(x);
  return {
    [=](string& s) -> delegated<ok_atom, string> {
      return self->delegate(x, std::move(s));
    }
  };
}

/******************************************************************************
 *                        sending typed actor handles                         *
 ******************************************************************************/

using int_actor = typed_actor<replies_to<int>::with<int>>;

int_actor::behavior_type int_fun() {
  return {
    [](int i) { return i * i; }
  };
}

behavior foo(event_based_actor* self) {
  return {
    [=](int i, int_actor server) {
      return self->request(server, i).then([=](int result) -> int {
        self->quit(exit_reason::normal);
        return result;
      });
    }
  };
}

int_actor::behavior_type int_fun2(int_actor::pointer self) {
  self->trap_exit(true);
  return {
    [=](int i) {
      self->monitor(self->current_sender());
      return i * i;
    },
    [=](const down_msg& dm) {
      CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
      self->quit();
    },
    [=](const exit_msg&) {
      CAF_TEST_ERROR("Unexpected message");
    }
  };
}

behavior foo2(event_based_actor* self) {
  return {
    [=](int i, int_actor server) {
      return self->request(server, i).then([=](int result) -> int {
        self->quit(exit_reason::normal);
        return result;
      });
    }
  };
}

struct fixture {
  actor_system system;

  fixture() : system(actor_system_config()
                     .add_message_type<get_state_msg>("get_state_msg")) {
    // nop
  }

  ~fixture() {
    system.await_all_actors_done();
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(typed_spawn_tests, fixture)

/******************************************************************************
 *                             put it all together                            *
 ******************************************************************************/

CAF_TEST(typed_spawns) {
  // run test series with typed_server(1|2)
  test_typed_spawn(system.spawn(typed_server1));
  system.await_all_actors_done();
  CAF_MESSAGE("finished test series with `typed_server1`");

  test_typed_spawn(system.spawn(typed_server2));
  system.await_all_actors_done();
  CAF_MESSAGE("finished test series with `typed_server2`");
  {
    scoped_actor self{system};
    test_typed_spawn(self->spawn<typed_server3>("hi there", self));
    self->receive(on("hi there") >> [] {
      CAF_MESSAGE("received \"hi there\"");
    });
  }
}

CAF_TEST(event_testee_series) {
  // run test series with event_testee
  scoped_actor self{system};
  auto et = self->spawn<event_testee>();
  string result;
  self->send(et, 1);
  self->send(et, 2);
  self->send(et, 3);
  self->send(et, .1f);
  self->send(et, "hello event testee!");
  self->send(et, .2f);
  self->send(et, .3f);
  self->send(et, "hello again event testee!");
  self->send(et, "goodbye event testee!");
  typed_actor<replies_to<get_state_msg>::with<string>> sub_et = et;
  // $:: is the anonymous namespace
  set<string> iface{"caf::replies_to<get_state_msg>::with<@str>",
                    "caf::replies_to<@str>::with<void>",
                    "caf::replies_to<float>::with<void>",
                    "caf::replies_to<@i32>::with<@i32>"};
  CAF_CHECK_EQUAL(join(sub_et->message_types(), ","), join(iface, ","));
  self->send(sub_et, get_state_msg{});
  // we expect three 42s
  int i = 0;
  self->receive_for(i, 3)([](int value) { CAF_CHECK_EQUAL(value, 42); });
  self->receive(
    [&](const string& str) {
      result = str;
    },
    after(chrono::minutes(1)) >> [&] {
      CAF_TEST_ERROR("event_testee does not reply");
      throw runtime_error("event_testee does not reply");
    }
  );
  self->send_exit(et, exit_reason::user_shutdown);
  self->await_all_other_actors_done();
  CAF_CHECK_EQUAL(result, "wait4int");
}

CAF_TEST(reverter_relay_chain) {
  // run test series with string reverter
  scoped_actor self{system};
  // actor-under-test
  auto aut = self->spawn<monitored>(string_relay,
                                          system.spawn(string_reverter),
                                          true);
  set<string> iface{"caf::replies_to<@str>::with<@str>"};
  CAF_CHECK(aut->message_types() == iface);
  self->request(aut, "Hello World!").await(
    [](const string& answer) {
      CAF_CHECK_EQUAL(answer, "!dlroW olleH");
    }
  );
  anon_send_exit(aut, exit_reason::user_shutdown);
}

CAF_TEST(string_delegator_chain) {
  // run test series with string reverter
  scoped_actor self{system};
  // actor-under-test
  auto aut = self->spawn<monitored>(string_delegator,
                                          system.spawn(string_reverter),
                                          true);
  set<string> iface{"caf::replies_to<@str>::with<@str>"};
  CAF_CHECK(aut->message_types() == iface);
  self->request(aut, "Hello World!").await(
    [](const string& answer) {
      CAF_CHECK_EQUAL(answer, "!dlroW olleH");
    }
  );
  anon_send_exit(aut, exit_reason::user_shutdown);
}

CAF_TEST(maybe_string_delegator_chain) {
  scoped_actor self{system};
  CAF_LOG_TRACE(CAF_ARG(self));
  auto aut = system.spawn(maybe_string_delegator,
                          system.spawn(maybe_string_reverter));
  CAF_MESSAGE("send empty string, expect error");
  self->request(aut, "").await(
    [](ok_atom, const string&) {
      throw std::logic_error("unexpected result!");
    },
    [](const error& err) {
      CAF_CHECK(err.category() == atom("mock"));
      CAF_CHECK_EQUAL(err.code(),
                      static_cast<uint8_t>(mock_errc::cannot_revert_empty));
    }
  );
  CAF_MESSAGE("send abcd string, expect dcba");
  self->request(aut, "abcd").await(
    [](ok_atom, const string& str) {
      CAF_CHECK_EQUAL(str, "dcba");
    },
    [](const error&) {
      throw std::logic_error("unexpected error!");
    }
  );
  anon_send_exit(aut, exit_reason::user_shutdown);
}

CAF_TEST(sending_typed_actors) {
  scoped_actor self{system};
  auto aut = system.spawn(int_fun);
  self->send(self->spawn(foo), 10, aut);
  self->receive(
    [](int i) {
      CAF_CHECK_EQUAL(i, 100);
    }
  );
  self->send_exit(aut, exit_reason::user_shutdown);
}

CAF_TEST(sending_typed_actors_and_down_msg) {
  scoped_actor self{system};
  auto aut = system.spawn(int_fun2);
  self->send(self->spawn(foo2), 10, aut);
  self->receive([](int i) {
    CAF_CHECK_EQUAL(i, 100);
  });
}

CAF_TEST(check_signature) {
  using foo_type = typed_actor<replies_to<put_atom>::with<ok_atom>>;
  using foo_result_type = maybe<ok_atom>;
  using bar_type = typed_actor<reacts_to<ok_atom>>;
  auto foo_action = [](foo_type::pointer self) -> foo_type::behavior_type {
    return {
      [=] (put_atom) -> foo_result_type {
        self->quit();
        return {ok_atom::value};
      }
    };
  };
  auto bar_action = [=](bar_type::pointer self) -> bar_type::behavior_type {
    auto foo = self->spawn<linked>(foo_action);
    self->send(foo, put_atom::value);
    return {
      [=](ok_atom) {
        self->quit();
      }
    };
  };
  scoped_actor self{system};
  self->spawn<monitored>(bar_action);
  self->receive(
    [](const down_msg& dm) {
      CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
    }
  );
}

namespace {

using first_stage = typed_actor<replies_to<int>::with<double, double>>;
using second_stage = typed_actor<replies_to<double, double>::with<double>>;

first_stage::behavior_type first_stage_impl() {
  return [](int i) {
    return std::make_tuple(i * 2.0, i * 4.0);
  };
};

second_stage::behavior_type second_stage_impl() {
  return [](double x, double y) {
    return x * y;
  };
}

} // namespace <anonymous>

CAF_TEST(dot_composition) {
  actor_system system;
  auto first = system.spawn(first_stage_impl);
  auto second = system.spawn(second_stage_impl);
  auto first_then_second = first * second;
  scoped_actor self{system};
  self->request(first_then_second, 42).await(
    [](double res) {
      CAF_CHECK(res == (42 * 2.0) * (42 * 4.0));
    }
  );
}

CAF_TEST(currying) {
  using namespace std::placeholders;
  auto impl = []() -> behavior {
    return {
      [](ok_atom, int x) {
        return x;
      },
      [](ok_atom, double x) {
        return x;
      }
    };
  };
  actor_system system;
  auto aut = system.spawn(impl);
  CAF_CHECK(system.registry().running() == 1);
  auto bound = aut.bind(ok_atom::value, _1);
  CAF_CHECK(aut.id() == bound.id());
  CAF_CHECK(aut.node() == bound.node());
  CAF_CHECK(aut == bound);
  CAF_CHECK(system.registry().running() == 1);
  scoped_actor self{system};
  CAF_CHECK(system.registry().running() == 2);
  self->request(bound, 2.0).await(
    [](double y) {
      CAF_CHECK(y == 2.0);
    }
  );
  self->request(bound, 10).await(
    [](int y) {
      CAF_CHECK(y == 10);
    }
  );
  self->send_exit(bound, exit_reason::kill);
  self->await_all_other_actors_done();
}

CAF_TEST(type_safe_currying) {
  using namespace std::placeholders;
  using testee = typed_actor<replies_to<ok_atom, int>::with<int>,
                             replies_to<ok_atom, double>::with<double>>;
  auto impl = []() -> testee::behavior_type {
    return {
      [](ok_atom, int x) {
        return x;
      },
      [](ok_atom, double x) {
        return x;
      }
    };
  };
  actor_system system;
  auto aut = system.spawn(impl);
  CAF_CHECK(system.registry().running() == 1);
  using curried_signature = typed_actor<replies_to<int>::with<int>,
                                        replies_to<double>::with<double>>;
  //auto bound = actor_bind(aut, ok_atom::value, _1);
  auto bound = aut.bind(ok_atom::value, _1);
  CAF_CHECK(aut == bound);
  CAF_CHECK(system.registry().running() == 1);
  static_assert(std::is_same<decltype(bound), curried_signature>::value,
                "bind returned wrong actor handle");
  scoped_actor self{system};
  CAF_CHECK(system.registry().running() == 2);
  self->request(bound, 2.0).await(
    [](double y) {
      CAF_CHECK(y == 2.0);
    }
  );
  self->request(bound, 10).await(
    [](int y) {
      CAF_CHECK(y == 10);
    }
  );
  self->send_exit(bound, exit_reason::kill);
  self->await_all_other_actors_done();
}

CAF_TEST(reordering) {
  auto impl = []() -> behavior {
    return {
      [](int x, double y) {
        return x * y;
      }
    };
  };
  actor_system system;
  auto aut = system.spawn(impl);
  CAF_CHECK(system.registry().running() == 1);
  using namespace std::placeholders;
  auto bound = aut.bind(_2, _1);
  CAF_CHECK(aut == bound);
  CAF_CHECK(system.registry().running() == 1);
  scoped_actor self{system};
  CAF_CHECK(system.registry().running() == 2);
  self->request(bound, 2.0, 10).await(
    [](double y) {
      CAF_CHECK(y == 20.0);
    }
  );
  self->send_exit(bound, exit_reason::kill);
  self->await_all_other_actors_done();
}

CAF_TEST(type_safe_reordering) {
  using testee = typed_actor<replies_to<int, double>::with<double>>;
  auto impl = []() -> testee::behavior_type {
    return {
      [](int x, double y) {
        return x * y;
      }
    };
  };
  actor_system system;
  auto aut = system.spawn(impl);
  CAF_CHECK(system.registry().running() == 1);
  using namespace std::placeholders;
  using swapped_signature = typed_actor<replies_to<double, int>::with<double>>;
  auto bound = aut.bind(_2, _1);
  CAF_CHECK(aut == bound);
  CAF_CHECK(system.registry().running() == 1);
  static_assert(std::is_same<decltype(bound), swapped_signature>::value,
                "bind returned wrong actor handle");
  scoped_actor self{system};
  CAF_CHECK(system.registry().running() == 2);
  self->request(bound, 2.0, 10).await(
    [](double y) {
      CAF_CHECK(y == 20.0);
    }
  );
  self->send_exit(bound, exit_reason::kill);
  self->await_all_other_actors_done();
}

CAF_TEST_FIXTURE_SCOPE_END()

#endif // CAF_WINDOWS
