/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#define ERROR_HANDLER                                                          \
  [&](error& err) { CAF_FAIL(system.render(err)); }

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

//static_assert(!std::is_convertible<dummy1, dummy2>::value,
//              "handle is assignable to broader definition");

using dummy3 = typed_actor<reacts_to<float, int>>;
using dummy4 = typed_actor<replies_to<int>::with<double>>;
using dummy5 = dummy4::extend_with<dummy3>;

static_assert(std::is_convertible<dummy5, dummy3>::value,
              "handle not assignable to narrower definition");

static_assert(std::is_convertible<dummy5, dummy4>::value,
              "handle not assignable to narrower definition");

//static_assert(!std::is_convertible<dummy3, dummy5>::value,
//              "handle is assignable to broader definition");

//static_assert(!std::is_convertible<dummy4, dummy5>::value,
//              "handle is assignable to broader definition");

/******************************************************************************
 *                        simple request/response test                        *
 ******************************************************************************/

struct my_request {
  int a;
  int b;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, my_request& x) {
  return f(x.a, x.b);
}

using server_type = typed_actor<replies_to<my_request>::with<bool>>;

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
    anon_send(buddy, line);
  }

  behavior_type make_behavior() override {
    return typed_server2(this);
  }
};

void client(event_based_actor* self, const actor& parent, const server_type& serv) {
  self->request(serv, infinite, my_request{0, 0}).then(
    [=](bool val1) {
      CAF_CHECK_EQUAL(val1, true);
      self->request(serv, infinite, my_request{10, 20}).then(
        [=](bool val2) {
          CAF_CHECK_EQUAL(val2, false);
          self->send(parent, passed_atom::value);
        }
      );
    }
  );
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
    return {
      [](const get_state_msg&) {
        return "wait4string";
      },
      [=](const string&) {
        become(wait4int());
      },
      [](float) {
        return skip();
      },
      [](int) {
        return skip();
      }
    };
  }

  behavior_type wait4int() {
    return {
      [](const get_state_msg&) {
        return "wait4int";
      },
      [=](int) -> int {
        become(wait4float());
        return 42;
      },
      [](float) {
        return skip();
      },
      [](const string&) {
        return skip();
      }
    };
  }

  behavior_type wait4float() {
    return {
      [](const get_state_msg&) {
        return "wait4float";
      },
      [=](float) {
        become(wait4string());
      },
      [](const string&) {
        return skip();
      },
      [](int) {
        return skip();
      }
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
    [](string& str) -> result<ok_atom, string> {
      if (str.empty())
        return mock_errc::cannot_revert_empty;
      std::reverse(str.begin(), str.end());
      return {ok_atom::value, std::move(str)};
    }
  };
}

maybe_string_actor::behavior_type
maybe_string_delegator(maybe_string_actor::pointer self, const maybe_string_actor& x) {
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

using float_actor = typed_actor<reacts_to<float>>;

int_actor::behavior_type int_fun() {
  return {
    [](int i) { return i * i; }
  };
}

behavior foo(event_based_actor* self) {
  return {
    [=](int i, int_actor server) {
      self->delegate(server, i);
      self->quit();
    }
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
    }
  };
}

float_actor::behavior_type float_fun(float_actor::pointer self) {
  return {
    [=](float a) {
      CAF_CHECK_EQUAL(a, 1.0f);
      self->quit(exit_reason::user_shutdown);
    }
  };
}

int_actor::behavior_type foo3(int_actor::pointer self) {
  auto b = self->spawn<linked>(float_fun);
  self->send(b, 1.0f);
  return {
    [=](int) {
      return 0;
    }
  };
}

struct fixture {
  actor_system_config cfg;
  actor_system system;
  scoped_actor self;

  static actor_system_config& init(actor_system_config& cfg) {
    cfg.add_message_type<get_state_msg>("get_state_msg");
    cfg.parse(test::engine::argc(), test::engine::argv());
    return cfg;
  }

  fixture() : system(init(cfg)), self(system) {
    // nop
  }

  void test_typed_spawn(server_type ts) {
    self->send(ts, my_request{1, 2});
    self->receive(
      [](bool value) {
        CAF_CHECK_EQUAL(value, false);
      }
    );
    CAF_MESSAGE("async send + receive");
    self->send(ts, my_request{42, 42});
    self->receive(
      [](bool value) {
        CAF_CHECK_EQUAL(value, true);
      }
    );
    CAF_MESSAGE("request + receive with result true");
    self->request(ts, infinite, my_request{10, 20}).receive(
      [](bool value) {
        CAF_CHECK_EQUAL(value, false);
      },
      ERROR_HANDLER
    );
    CAF_MESSAGE("request + receive with result false");
    self->request(ts, infinite, my_request{0, 0}).receive(
      [](bool value) {
        CAF_CHECK_EQUAL(value, true);
      },
      ERROR_HANDLER
    );
    CAF_CHECK_EQUAL(system.registry().running(), 2u);
    auto c1 = self->spawn(client, self, ts);
    self->receive(
      [](passed_atom) {
        CAF_MESSAGE("received `passed_atom`");
      }
    );
    self->wait_for(c1);
    CAF_CHECK_EQUAL(system.registry().running(), 2u);
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(typed_spawn_tests, fixture)

/******************************************************************************
 *                             put it all together                            *
 ******************************************************************************/

CAF_TEST(typed_spawns) {
  CAF_MESSAGE("run test series with typed_server1");
  test_typed_spawn(system.spawn(typed_server1));
  self->await_all_other_actors_done();
  CAF_MESSAGE("finished test series with `typed_server1`");
  CAF_MESSAGE("run test series with typed_server2");
  test_typed_spawn(system.spawn(typed_server2));
  self->await_all_other_actors_done();
  CAF_MESSAGE("finished test series with `typed_server2`");
  test_typed_spawn(self->spawn<typed_server3>("hi there", self));
  self->receive(
    [](const string& str) {
      CAF_REQUIRE_EQUAL(str, "hi there");
    }
  );
}

CAF_TEST(event_testee_series) {
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
  std::set<string> iface{"caf::replies_to<get_state_msg>::with<@str>",
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
      CAF_FAIL("event_testee does not reply");
    }
  );
  CAF_CHECK_EQUAL(result, "wait4int");
}

CAF_TEST(string_delegator_chain) {
  // run test series with string reverter
  auto aut = self->spawn<monitored>(string_delegator,
                                    system.spawn(string_reverter),
                                    true);
  set<string> iface{"caf::replies_to<@str>::with<@str>"};
  CAF_CHECK_EQUAL(aut->message_types(), iface);
  self->request(aut, infinite, "Hello World!").receive(
    [](const string& answer) {
      CAF_CHECK_EQUAL(answer, "!dlroW olleH");
    },
    ERROR_HANDLER
  );
}

CAF_TEST(maybe_string_delegator_chain) {
  CAF_LOG_TRACE(CAF_ARG(self));
  auto aut = system.spawn(maybe_string_delegator,
                          system.spawn(maybe_string_reverter));
  CAF_MESSAGE("send empty string, expect error");
  self->request(aut, infinite, "").receive(
    [](ok_atom, const string&) {
      CAF_FAIL("unexpected string response");
    },
    [](const error& err) {
      CAF_CHECK_EQUAL(err.category(), atom("mock"));
      CAF_CHECK_EQUAL(err.code(),
                      static_cast<uint8_t>(mock_errc::cannot_revert_empty));
    }
  );
  CAF_MESSAGE("send abcd string, expect dcba");
  self->request(aut, infinite, "abcd").receive(
    [](ok_atom, const string& str) {
      CAF_CHECK_EQUAL(str, "dcba");
    },
    ERROR_HANDLER
  );
}

CAF_TEST(sending_typed_actors) {
  auto aut = system.spawn(int_fun);
  self->send(self->spawn(foo), 10, aut);
  self->receive(
    [](int i) {
      CAF_CHECK_EQUAL(i, 100);
    }
  );
  self->spawn(foo3);
}

CAF_TEST(sending_typed_actors_and_down_msg) {
  auto aut = system.spawn(int_fun2);
  self->send(self->spawn(foo2), 10, aut);
  self->receive([](int i) {
    CAF_CHECK_EQUAL(i, 100);
  });
}

CAF_TEST(check_signature) {
  using foo_type = typed_actor<replies_to<put_atom>::with<ok_atom>>;
  using foo_result_type = optional<ok_atom>;
  using bar_type = typed_actor<reacts_to<ok_atom>>;
  auto foo_action = [](foo_type::pointer ptr) -> foo_type::behavior_type {
    return {
      [=] (put_atom) -> foo_result_type {
        ptr->quit();
        return {ok_atom::value};
      }
    };
  };
  auto bar_action = [=](bar_type::pointer ptr) -> bar_type::behavior_type {
    auto foo = ptr->spawn<linked>(foo_action);
    ptr->send(foo, put_atom::value);
    return {
      [=](ok_atom) {
        ptr->quit();
      }
    };
  };
  auto x = self->spawn(bar_action);
  self->wait_for(x);
}

CAF_TEST_FIXTURE_SCOPE_END()

#endif // CAF_WINDOWS
