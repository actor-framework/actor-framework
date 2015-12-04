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

CAF_TEST(test_event_testee) {
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

CAF_TEST(test_sending_typed_actors) {
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

CAF_TEST(test_sending_typed_actors_and_down_msg) {
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

using caf::detail::type_list;

template <class X, class Y>
struct typed_actor_combine_one {
  using type = void;
};

template <class... Xs, class... Ys, class... Zs>
struct typed_actor_combine_one<typed_mpi<type_list<Xs...>, type_list<Ys...>>,
                               typed_mpi<type_list<Ys...>, type_list<Zs...>>> {
  using type = typed_mpi<type_list<Xs...>, type_list<Zs...>>;
};

template <class X, class Y>
struct typed_actor_combine_all;

template <class X, class... Ys>
struct typed_actor_combine_all<X, type_list<Ys...>> {
  using type = type_list<typename typed_actor_combine_one<X, Ys>::type...>;
};

template <class X, class Y>
struct type_actor_combine;

template <class... Xs, class... Ys>
struct type_actor_combine<typed_actor<Xs...>, typed_actor<Ys...>> {
  // store Ys in a packed list
  using ys = type_list<Ys...>;
  // combine each X with all Ys
  using all =
    typename detail::tl_concat<
      typename typed_actor_combine_all<Xs, ys>::type...
    >::type;
  // drop all mismatches (void results)
  using filtered = typename detail::tl_filter_not_type<all, void>::type;
  // throw error if we don't have a single match
  static_assert(detail::tl_size<filtered>::value > 0,
                "Left-hand actor type does not produce a single result which "
                "is valid as input to the right-hand actor type.");
  // compute final actor type
  using type = typename detail::tl_apply<filtered, typed_actor>::type;
};

template <class... Xs, class... Ys>
typename type_actor_combine<typed_actor<Xs...>, typed_actor<Ys...>>::type
operator*(const typed_actor<Xs...>& x, const typed_actor<Ys...>& y) {
  using res_type = typename type_actor_combine<typed_actor<Xs...>,
                                               typed_actor<Ys...>>::type;
  if (! x || ! y)
    return {};
  auto f = [=](event_based_actor* self) -> behavior {
    self->link_to(x);
    self->link_to(y);
    self->trap_exit(true);
    auto x_ = actor_cast<actor>(x);
    auto y_ = actor_cast<actor>(y);
    return {
      [=](const exit_msg& msg) {
        // also terminate for normal exit reasons
        if (msg.source == x || msg.source == y)
          self->quit(msg.reason);
      },
      others >> [=] {
        auto rp = self->make_response_promise();
        self->request(x_, self->current_message()).generic_then(
          [=](message& msg) {
            self->request(y_, std::move(msg)).generic_then(
              [=](message& msg) {
                rp.deliver(std::move(msg));
              },
              [=](error& err) {
                rp.deliver(std::move(err));
              }
            );
          },
          [=](error& err) {
            rp.deliver(std::move(err));
          }
        );
      }
    };
  };
  return actor_cast<res_type>(x->home_system().spawn(f));
}

using first_stage = typed_actor<replies_to<int>::with<double>>;
using second_stage = typed_actor<replies_to<double>::with<string>>;

first_stage::behavior_type first_stage_impl() {
  return [](int i) {
    return static_cast<double>(i) * 2;
  };
};

second_stage::behavior_type second_stage_impl() {
  return [](double x) {
    return std::to_string(x);
  };
}

CAF_TEST(composition) {
  actor_system system;
  auto first = system.spawn(first_stage_impl);
  auto second = system.spawn(second_stage_impl);
  auto first_then_second = first * second;
  scoped_actor self{system};
  self->request(first_then_second, 42).await(
    [](const string& str) {
      CAF_MESSAGE("received: " << str);
    }
  );
}

CAF_TEST_FIXTURE_SCOPE_END()

#endif // CAF_WINDOWS
