/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#define CAF_SUITE composable_behaviors
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

#define ERROR_HANDLER                                                          \
  [&](error& err) { CAF_FAIL(system.render(err)); }

using namespace std;
using namespace caf;

namespace {

// -- composable behaviors using primitive data types --------------------------

using i3_actor = typed_actor<replies_to<int, int, int>::with<int>>;

using d_actor = typed_actor<replies_to<double>::with<double, double>>;

using foo_actor = i3_actor::extend_with<d_actor>;

class foo_actor_state : public composable_behavior<foo_actor> {
public:
  result<int> operator()(int x, int y, int z) override {
    return x + y + z;
  }

  result<double, double> operator()(double x) override {
    return {x, x};
  }
};

class i3_actor_state : public composable_behavior<i3_actor> {
public:
  result<int> operator()(int x, int y, int z) override {
    return x + y + z;
  }
};

class d_actor_state : public composable_behavior<d_actor> {
public:
  result<double, double> operator()(double x) override {
    return {x, x};
  }
};

class i3_actor_state2 : public composable_behavior<i3_actor> {
public:
  result<int> operator()(int x, int y, int z) override {
    return x * (y * z);
  }
};

// checks whether CAF resolves "diamonds" properly by inheriting
// from two behaviors that both implement i3_actor
struct foo_actor_state2
    : composed_behavior<i3_actor_state2, i3_actor_state, d_actor_state> {
  result<int> operator()(int x, int y, int z) override {
    return x - y - z;
  }
};

// -- composable behaviors using param<T> arguments ----------------------------

std::atomic<long> counting_strings_created;
std::atomic<long> counting_strings_moved;
std::atomic<long> counting_strings_destroyed;

// counts how many instances where created
struct counting_string {
public:
  counting_string() {
    ++counting_strings_created;
  }

  counting_string(const char* cstr) : str_(cstr) {
    ++counting_strings_created;
  }

  counting_string(const counting_string& x) : str_(x.str_) {
    ++counting_strings_created;
  }

  counting_string(counting_string&& x) : str_(std::move(x.str_)) {
    ++counting_strings_created;
    ++counting_strings_moved;
  }

  ~counting_string() {
    ++counting_strings_destroyed;
  }

  counting_string& operator=(const char* cstr) {
    str_ = cstr;
    return *this;
  }

  const std::string& str() const {
    return str_;
  }

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f,
                                                 counting_string& x) {
    return f(x.str_);
  }

private:
  std::string str_;
};

bool operator==(const counting_string& x, const counting_string& y) {
  return x.str() == y.str();
}

bool operator==(const counting_string& x, const char* y) {
  return x.str() == y;
}

std::string to_string(const counting_string& ref) {
  return ref.str();
}

} // namespace <anonymous>

namespace std {

template <>
struct hash<counting_string> {
  inline size_t operator()(const counting_string& ref) const {
    hash<string> f;
    return f(ref.str());
  }
};

} // namespace std

namespace {

using add_atom = atom_constant<atom("add")>;
using get_name_atom = atom_constant<atom("getName")>;
using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;

// "base" interface
using named_actor =
  typed_actor<replies_to<get_name_atom>::with<counting_string>,
              replies_to<ping_atom>::with<pong_atom>>;

// a simple dictionary
using dict =
  named_actor::extend<replies_to<get_atom, counting_string>
                      ::with<counting_string>,
                      replies_to<put_atom, counting_string, counting_string>
                      ::with<void>>;

class dict_state : public composable_behavior<dict> {
public:
  result<counting_string> operator()(get_name_atom) override {
    return "dictionary";
  }

  result<pong_atom> operator()(ping_atom) override {
    return pong_atom::value;
  }

  result<counting_string> operator()(get_atom,
                                     param<counting_string> key) override {
    auto i = values_.find(key.get());
    if (i == values_.end())
      return "";
    return i->second;
  }

  result<void> operator()(put_atom, param<counting_string> key,
                          param<counting_string> value) override {
    if (values_.count(key.get()) != 0)
      return unit;
    values_.emplace(key.move(), value.move());
    return unit;
  }

protected:
  std::unordered_map<counting_string, counting_string> values_;
};

using delayed_testee_actor = typed_actor<reacts_to<int>,
                                         replies_to<bool>::with<int>,
                                         reacts_to<std::string>>;

class delayed_testee : public composable_behavior<delayed_testee_actor> {
public:
  result<void> operator()(int x) override {
    CAF_CHECK_EQUAL(x, 42);
    self->delayed_anon_send(self, std::chrono::milliseconds(10), true);
    return unit;
  }

  result<int> operator()(bool x) override {
    CAF_CHECK_EQUAL(x, true);
    self->delayed_send(self, std::chrono::milliseconds(10), "hello");
    return 0;
  }

  result<void> operator()(param<std::string> x) override {
    CAF_CHECK_EQUAL(x.get(), "hello");
    return unit;
  }
};

struct fixture {
  fixture() : system(cfg) {
    // nop
  }

  actor_system_config cfg;
  actor_system system;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(composable_behaviors_tests, fixture)

CAF_TEST(composition) {
  // test foo_foo_actor_state
  auto f1 = make_function_view(system.spawn<foo_actor_state>());
  CAF_CHECK_EQUAL(f1(1, 2, 4), 7);
  CAF_CHECK_EQUAL(f1(42.0), std::make_tuple(42.0, 42.0));
  // test on-the-fly composition of i3_actor_state and d_actor_state
  f1.assign(system.spawn<composed_behavior<i3_actor_state, d_actor_state>>());
  CAF_CHECK_EQUAL(f1(1, 2, 4), 7);
  CAF_CHECK_EQUAL(f1(42.0), std::make_tuple(42.0, 42.0));
  // test on-the-fly composition of i3_actor_state2 and d_actor_state
  f1.assign(system.spawn<composed_behavior<i3_actor_state2, d_actor_state>>());
  CAF_CHECK_EQUAL(f1(1, 2, 4), 8);
  CAF_CHECK_EQUAL(f1(42.0), std::make_tuple(42.0, 42.0));
  // test foo_actor_state2
  f1.assign(system.spawn<foo_actor_state2>());
  CAF_CHECK_EQUAL(f1(1, 2, 4), -5);
  CAF_CHECK_EQUAL(f1(42.0), std::make_tuple(42.0, 42.0));
}

CAF_TEST(param_detaching) {
  auto dict = actor_cast<actor>(system.spawn<dict_state>());
  scoped_actor self{system};
  // this ping-pong makes sure that dict has cleaned up all state related
  // to a test before moving to the second test; otherwise, reference counts
  // can diverge from what we expect
  auto ping_pong = [&] {
    self->request(dict, infinite, ping_atom::value).receive(
      [](pong_atom) {
        // nop
      },
      [&](error& err) {
        CAF_FAIL("error: " << system.render(err));
      }
    );
  };
  // Using CAF is the key to success!
  counting_string key = "CAF";
  counting_string value = "success";
  CAF_CHECK_EQUAL(counting_strings_created.load(), 2);
  CAF_CHECK_EQUAL(counting_strings_moved.load(), 0);
  CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 0);
  // wrap two strings into messages
  auto put_msg = make_message(put_atom::value, key, value);
  auto get_msg = make_message(get_atom::value, key);
  CAF_CHECK_EQUAL(counting_strings_created.load(), 5);
  CAF_CHECK_EQUAL(counting_strings_moved.load(), 0);
  CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 0);
  // send put message to dictionary
  self->request(dict, infinite, put_msg).receive(
    [&] {
      ping_pong();
      // the handler of put_atom calls .move() on key and value,
      // both causing to detach + move into the map
      CAF_CHECK_EQUAL(counting_strings_created.load(), 9);
      CAF_CHECK_EQUAL(counting_strings_moved.load(), 2);
      CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 2);
    },
    ERROR_HANDLER
  );
  // send put message to dictionary again
  self->request(dict, infinite, put_msg).receive(
    [&] {
      ping_pong();
      // the handler checks whether key already exists -> no copies
      CAF_CHECK_EQUAL(counting_strings_created.load(), 9);
      CAF_CHECK_EQUAL(counting_strings_moved.load(), 2);
      CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 2);
    },
    ERROR_HANDLER
  );
  // alter our initial put, this time moving it to the dictionary
  put_msg.get_mutable_as<counting_string>(1) = "neverlord";
  put_msg.get_mutable_as<counting_string>(2) = "CAF";
  // send put message to dictionary
  self->request(dict, infinite, std::move(put_msg)).receive(
    [&] {
      ping_pong();
      // the handler of put_atom calls .move() on key and value,
      // but no detaching occurs this time (unique access) -> move into the map
      CAF_CHECK_EQUAL(counting_strings_created.load(), 11);
      CAF_CHECK_EQUAL(counting_strings_moved.load(), 4);
      CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 4);
    },
    ERROR_HANDLER
  );
  // finally, check for original key
  self->request(dict, infinite, std::move(get_msg)).receive(
    [&](const counting_string& str) {
      ping_pong();
      // we receive a copy of the value, which is copied out of the map and
      // then moved into the result message;
      // the string from our get_msg is destroyed
      CAF_CHECK_EQUAL(counting_strings_created.load(), 13);
      CAF_CHECK_EQUAL(counting_strings_moved.load(), 5);
      CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 6);
      CAF_CHECK_EQUAL(str, "success");
    },
    ERROR_HANDLER
  );
  // temporary of our handler is destroyed
  CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 7);
  self->send_exit(dict, exit_reason::kill);
  self->await_all_other_actors_done();
  // only `key` and `value` from this scope remain
  CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 11);
}

CAF_TEST(delayed_sends) {
  scoped_actor self{system};
  auto testee = self->spawn<delayed_testee>();
  self->send(testee, 42);
}

CAF_TEST_FIXTURE_SCOPE_END()

CAF_TEST(dynamic_spawning) {
  using impl = composable_behavior_based_actor<foo_actor_state>;
  actor_system_config cfg;
  cfg.add_actor_type<impl>("foo_actor");
  actor_system sys{cfg};
  auto sr = sys.spawn<foo_actor>("foo_actor", make_message());
  CAF_REQUIRE(sr);
  auto f1 = make_function_view(std::move(*sr));
  CAF_CHECK_EQUAL(f1(1, 2, 4), 7);
  CAF_CHECK_EQUAL(f1(42.0), std::make_tuple(42.0, 42.0));
}
