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

using dummy3 = typed_actor<reacts_to<float, int>>;
using dummy4 = typed_actor<replies_to<int>::with<double>>;
using dummy5 = dummy4::extend_with<dummy3>;

static_assert(std::is_convertible<dummy5, dummy3>::value,
              "handle not assignable to narrower definition");

static_assert(std::is_convertible<dummy5, dummy4>::value,
              "handle not assignable to narrower definition");

static_assert(! std::is_convertible<dummy3, dummy5>::value,
              "handle is assignable to broader definition");

static_assert(! std::is_convertible<dummy4, dummy5>::value,
              "handle is assignable to broader definition");


// mockup

template <class... Sigs>
class typed_pointer_view {
public:
  template <class Supertype>
  typed_pointer_view(Supertype* selfptr) : ptr_(selfptr) {
    using namespace caf::detail;
    static_assert(tlf_is_subset(type_list<Sigs...>{},
                                typename Supertype::signatures{}),
                  "cannot create a pointer view to an unrelated actor type");
  }

  typed_pointer_view(const nullptr_t&) : ptr_(nullptr) {
    // nop
  }

  inline typed_pointer_view* operator->() {
    return this;
  }

private:
  local_actor* ptr_;
};

template <class...>
struct mpi_output;

template <class T>
struct mpi_output<T> {
  using type = T;
};

template <class T0, class T1, class... Ts>
struct mpi_output<T0, T1, Ts...> {
  using type = std::tuple<T0, T1, Ts...>;
};

template <class... Ts>
using mpi_output_t = typename mpi_output<Ts...>::type;

template <class T, bool IsArithmetic = std::is_arithmetic<T>::value>
struct mpi_input {
  using type = const T&;
};

template <class T>
struct mpi_input<T, true> {
  using type = T;
};

template <atom_value X>
struct mpi_input<atom_constant<X>, false> {
  using type = atom_constant<X>;
};

template <class T>
using mpi_input_t = typename mpi_input<T>::type;

/// Generates an interface class that provides `operator()`. The signature
/// of the apply operator is derived from the typed message passing interface
/// `MPI`.
template <class MPI>
class abstract_composable_state_mixin;

template <class... Xs, class... Ys>
class abstract_composable_state_mixin<typed_mpi<detail::type_list<Xs...>,
                                       detail::type_list<Ys...>>> {
public:
  virtual ~abstract_composable_state_mixin() noexcept {
    // nop
  }

  virtual mpi_output_t<Ys...> operator()(mpi_input_t<Xs>...) = 0;
};

/// Marker type that allows CAF to spawn actors from composable states.
class abstract_composable_state {
public:
  virtual ~abstract_composable_state() noexcept {
    // nop
  }

  virtual void init_behavior(behavior& x) = 0;
};

using caf::detail::type_list;

using caf::detail::tl_apply;
using caf::detail::tl_union;
using caf::detail::tl_intersect;

template <class T, class... Fs>
void init_behavior_impl(T*, type_list<>, behavior& storage, Fs... fs) {
  storage.assign(std::move(fs)...);
}

template <class T, class... Xs, class... Ys, class... Ts, class... Fs>
void init_behavior_impl(T* thisptr,
                        type_list<typed_mpi<type_list<Xs...>,
                                            type_list<Ys...>>,
                                  Ts...>,
                        behavior& storage, Fs... fs) {
  auto f = [=](mpi_input_t<Xs>... xs) {
    return (*thisptr)(xs...);
  };
  type_list<Ts...> token;
  init_behavior_impl(thisptr, token, storage, fs..., f);
}

/// Base type for composable actor states.
template <class TypedActor>
class composable_state;

template <class... Clauses>
class composable_state<typed_actor<Clauses...>> : virtual public abstract_composable_state,
                                            public abstract_composable_state_mixin<Clauses>... {
public:
  using signatures = detail::type_list<Clauses...>;

  using handle_type =
    typename tl_apply<
      signatures,
      typed_actor
    >::type;

  using actor_base = typename handle_type::base;

  using behavior_type = typename handle_type::behavior_type;

  composable_state() : self(nullptr) {
    // nop
  }

  template <class SelfPointer>
  void init_selfptr(SelfPointer selfptr) {
    self = selfptr;
  }

  void init_behavior(behavior& x) override {
    signatures token;
    init_behavior_impl(this, token, x);
  }

protected:
  typed_pointer_view<Clauses...> self;
};

template <class InterfaceIntersection, class... States>
class composed_state_base;

template <class T>
class composed_state_base<type_list<>, T> : public T {
public:
  using T::operator();

  // make this pure again, since the compiler can otherwise
  // runs into a "multiple final overriders" error
  virtual void init_behavior(behavior& x) override = 0;
};

template <class A, class B, class... Ts>
class composed_state_base<type_list<>, A, B, Ts...> : public A, public composed_state_base<type_list<>, B, Ts...> {
public:
  using super = composed_state_base<type_list<>, B, Ts...>;

  using A::operator();
  using super::operator();

  template <class SelfPtr>
  void init_selfptr(SelfPtr ptr) {
    A::init_selfptr(ptr);
    super::init_selfptr(ptr);
  }

  // make this pure again, since the compiler can otherwise
  // runs into a "multiple final overriders" error
  virtual void init_behavior(behavior& x) override = 0;
};

template <class... Xs, class... Ys, class... Ts, class... States>
class composed_state_base<type_list<typed_mpi<type_list<Xs...>,
                                              type_list<Ys...>>,
                                    Ts...>,
                          States...>
  : public composed_state_base<type_list<Ts...>, States...> {
public:
  using super = composed_state_base<type_list<Ts...>, States...>;

  using super::operator();

  virtual mpi_output_t<Ys...> operator()(mpi_input_t<Xs>...) override = 0;
};

template <class... Ts>
class composed_state
  : public composed_state_base<typename tl_intersect<typename Ts::
                                                       signatures...>::type,
                               Ts...> {
private:
  using super =
    composed_state_base<typename tl_intersect<typename Ts::signatures...>::type,
                        Ts...>;

public:
  using signatures = typename tl_union<typename Ts::signatures...>::type;

  using handle_type =
    typename tl_apply<
      signatures,
      typed_actor
    >::type;

  using behavior_type = typename handle_type::behavior_type;

  using combined_type = composed_state;

  using actor_base = typename handle_type::base;

  using self_pointer =
    typename tl_apply<
      signatures,
      typed_pointer_view
    >::type;

  composed_state() : self(nullptr) {
    // nop
  }

  template <class SelfPtr>
  void init_selfptr(SelfPtr ptr) {
    self = ptr;
    super::init_selfptr(ptr);
  }

  using super::operator();

  void init_behavior(behavior& x) override {
    signatures token;
    init_behavior_impl(this, token, x);
  }

protected:
  self_pointer self;
};

using i3_actor = typed_actor<replies_to<int, int, int>::with<int>>;

using d_actor = typed_actor<replies_to<double>::with<double, double>>;

using foo_actor = i3_actor::extend_with<d_actor>;

class foo_actor_state : public composable_state<foo_actor> {
public:
  int operator()(int x, int y, int z) override {
    return x + y + z;
  }

  std::tuple<double, double> operator()(double x) override {
    return std::make_tuple(x, x);
  }
};

class i3_actor_state : public composable_state<i3_actor> {
public:
  int operator()(int x, int y, int z) override {
    return x + y + z;
  }
};

class d_actor_state : public composable_state<d_actor> {
public:
  std::tuple<double, double> operator()(double x) override {
    return std::make_tuple(x, x);
  }
};

struct match_case_filter {
  template <class... Xs, class... Ts>
  void operator()(std::tuple<Xs...>& tup, Ts&... xs) const {
    std::integral_constant<size_t, 0> next_pos;
    detail::type_list<> next_token;
    (*this)(next_pos, tup, next_token, xs...);
  }

  template <size_t P, class Tuple, class... Consumed>
  void operator()(std::integral_constant<size_t, P>, Tuple&,
                  detail::type_list<Consumed...>) const {
    // end of recursion
  }

  template <size_t P, class Tuple, class... Consumed, class T, class... Ts>
  typename std::enable_if<
    detail::tl_index_of<detail::type_list<Consumed...>, T>::value == -1
  >::type
  operator()(std::integral_constant<size_t, P>, Tuple& tup,
             detail::type_list<Consumed...>, T& x, Ts&... xs) const {
    get<P>(tup) = std::move(x);
    std::integral_constant<size_t, P + 1> next_pos;
    detail::type_list<Consumed..., T> next_token;
    (*this)(next_pos, tup, next_token, xs...);
  }

  template <size_t P, class Tuple, class... Consumed, class T, class... Ts>
  typename std::enable_if<
    detail::tl_index_of<detail::type_list<Consumed...>, T>::value != -1
  >::type
  operator()(std::integral_constant<size_t, P> pos, Tuple& tup,
             detail::type_list<Consumed...> token, T&, Ts&... xs) const {
    (*this)(pos, tup, token, xs...);
  }
};

using detail::type_list;

class i3_actor_state2 : public composable_state<i3_actor> {
public:
  int operator()(int x, int y, int z) override {
    return x * (y * z);
  }
};

struct foo_actor_state2 : composed_state<i3_actor_state2, i3_actor_state, d_actor_state> {
  int operator()(int x, int y, int z) override {
    return x - y - z;
  }
};




using add_atom = atom_constant<atom("Add")>;
using get_name_atom = atom_constant<atom("GetName")>;

// "base" interface
using named_actor = typed_actor<replies_to<get_name_atom>::with<std::string>>;

// a simple dictionary
using dict = named_actor::extend<replies_to<get_atom, std::string>::with<std::string>,
                                 replies_to<put_atom, std::string, std::string>::with<void>>;

// a simple calculator
using calc = named_actor::extend<replies_to<add_atom, int, int>::with<int>>;

class dict_state : public composable_state<dict> {
public:
  std::string operator()(get_name_atom) override {
    return "dictionary";
  }

  std::string operator()(get_atom, const std::string& key) override {
    auto i = values_.find(key);
    if (i == values_.end())
      return "";
    return i->second;
  }

  void operator()(put_atom, const std::string& key, const std::string& value) override {
    values_[key] = value;
  }

protected:
  std::unordered_map<std::string, std::string> values_;
};

class calc_state : public composable_state<calc> {
public:
  std::string operator()(get_name_atom) override {
    return "calculator";
  }

  int operator()(add_atom, int x, int y) override {
    return x + y;
  }
};

class dict_calc_state : public composed_state<dict_state, calc_state> {
public:
  // composed_state<...> will mark this operator pure virtual, because
  // of conflicting declarations in dict_state and calc_state
  std::string operator()(get_name_atom) override {
    return "calculating dictionary";
  }
};

template <class State>
class stateful_impl : public stateful_actor<State, typename State::actor_base> {
public:
  static_assert(! std::is_abstract<State>::value,
                "State is abstract, please make sure to override all "
                "virtual operator() member functions");

  using super = stateful_actor<State, typename State::actor_base>;

  stateful_impl(actor_config& cfg) : super(cfg) {
    // nop
  }

  using behavior_type = typename State::behavior_type;

  behavior_type make_behavior() override {
    this->state.init_selfptr(this);
    behavior tmp;
    this->state.init_behavior(tmp);
    return behavior_type{typename behavior_type::unsafe_init{}, std::move(tmp)};
  }
};

result<int> test1() {
  return 42;
}

result<int, int> test2() {
  return {1, 2};
}

result<float> test3() {
  return skip_message();
}

result<int, int, int> test4() {
  return delegated<int, int, int>{};
}

result<float, float> test5() {
  return sec::state_not_serializable;
}

} // namespace <anonymous>

CAF_TEST(foobarz) {
  actor_system sys;
  auto x1 = sys.spawn<stateful_impl<foo_actor_state>>();
  scoped_actor self{sys};
  self->request(x1, 1, 2, 4).receive(
    [](int y) {
      CAF_CHECK(y == 7);
    }
  );
  self->send_exit(x1, exit_reason::kill);
  auto x2 = sys.spawn<stateful_impl<composed_state<i3_actor_state, d_actor_state>>>();
  self->request(x2, 1, 2, 4).receive(
    [](int y) {
      CAF_CHECK(y == 7);
    }
  );
  self->send_exit(x2, exit_reason::kill);
  auto x3 = sys.spawn<stateful_impl<foo_actor_state2>>();
  self->request(x3, 1, 2, 4).receive(
    [](int y) {
      CAF_CHECK(y == -5);
    }
  );
  self->send_exit(x3, exit_reason::kill);
  auto x4 = sys.spawn<stateful_impl<dict_calc_state>>();
  self->request(x4, add_atom::value, 10, 20).receive(
    [](int y) {
      CAF_CHECK(y == 30);
    }
  );
  self->send_exit(x4, exit_reason::kill);
}

namespace {



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
  self->request(ts, my_request{10, 20}).receive(
    [](bool value) {
      CAF_CHECK_EQUAL(value, false);
    }
  );
  self->request(ts, my_request{0, 0}).receive(
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
      self->delegate(server, i);
      self->quit();
      /*
      return self->request(server, i).then([=](int result) -> int {
        self->quit(exit_reason::normal);
        return result;
      });
      */
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
      CAF_ERROR("Unexpected message");
    }
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

struct fixture {
  actor_system system;

  fixture() : system(actor_system_config()
                     .add_message_type<get_state_msg>("get_state_msg")) {
    // nop
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
  scoped_actor self{system};
  test_typed_spawn(self->spawn<typed_server3>("hi there", self));
  self->receive(on("hi there") >> [] {
    CAF_MESSAGE("received \"hi there\"");
  });
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
      CAF_ERROR("event_testee does not reply");
      throw runtime_error("event_testee does not reply");
    }
  );
  self->send_exit(et, exit_reason::user_shutdown);
  self->await_all_other_actors_done();
  CAF_CHECK_EQUAL(result, "wait4int");
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
  self->request(aut, "Hello World!").receive(
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
  self->request(aut, "").receive(
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
  self->request(aut, "abcd").receive(
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

CAF_TEST_FIXTURE_SCOPE_END()

#endif // CAF_WINDOWS
