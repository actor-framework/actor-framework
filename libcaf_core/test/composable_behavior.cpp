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

#define CAF_SUITE composable_behavior

#include "caf/composable_behavior.hpp"

#include "caf/test/dsl.hpp"

#include "caf/attach_stream_sink.hpp"
#include "caf/attach_stream_source.hpp"
#include "caf/attach_stream_stage.hpp"
#include "caf/typed_actor.hpp"

#define ERROR_HANDLER [&](error& err) { CAF_FAIL(system.render(err)); }

using namespace caf;

namespace {

// -- composable behaviors using primitive data types and streams --------------

using i3_actor = typed_actor<replies_to<int, int, int>::with<int>>;

using d_actor = typed_actor<replies_to<double>::with<double, double>>;

using source_actor = typed_actor<replies_to<open_atom>::with<stream<int>>>;

using stage_actor = typed_actor<replies_to<stream<int>>::with<stream<int>>>;

using sink_actor = typed_actor<reacts_to<stream<int>>>;

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

class source_actor_state : public composable_behavior<source_actor> {
public:
  result<stream<int>> operator()(open_atom) override {
    return attach_stream_source(
      self, [](size_t& counter) { counter = 0; },
      [](size_t& counter, downstream<int>& out, size_t hint) {
        auto n = std::min(static_cast<size_t>(100 - counter), hint);
        for (size_t i = 0; i < n; ++i)
          out.push(counter++);
      },
      [](const size_t& counter) { return counter < 100; });
  }
};

class stage_actor_state : public composable_behavior<stage_actor> {
public:
  result<stream<int>> operator()(stream<int> in) override {
    return attach_stream_stage(
      self, in,
      [](unit_t&) {
        // nop
      },
      [](unit_t&, downstream<int>& out, int x) {
        if (x % 2 == 0)
          out.push(x);
      });
  }
};

class sink_actor_state : public composable_behavior<sink_actor> {
public:
  std::vector<int> buf;

  result<void> operator()(stream<int> in) override {
    attach_stream_sink(
      self, in,
      [](unit_t&) {
        // nop
      },
      [=](unit_t&, int x) { buf.emplace_back(x); });
    return unit;
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

} // namespace

namespace std {

template <>
struct hash<counting_string> {
  size_t operator()(const counting_string& ref) const {
    hash<string> f;
    return f(ref.str());
  }
};

} // namespace std

namespace {

// a simple dictionary
using dict = typed_actor<
  replies_to<get_atom, counting_string>::with<counting_string>,
  replies_to<put_atom, counting_string, counting_string>::with<void>>;

class dict_state : public composable_behavior<dict> {
public:
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

using delayed_testee_actor = typed_actor<
  reacts_to<int>, replies_to<bool>::with<int>, reacts_to<std::string>>;

class delayed_testee : public composable_behavior<delayed_testee_actor> {
public:
  result<void> operator()(int x) override {
    CAF_CHECK_EQUAL(x, 42);
    delayed_anon_send(self, std::chrono::milliseconds(10), true);
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

struct config : actor_system_config {
  config() {
    using foo_actor_impl = composable_behavior_based_actor<foo_actor_state>;
    add_actor_type<foo_actor_impl>("foo_actor");
  }
};

struct fixture : test_coordinator_fixture<config> {
  // nop
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(composable_behaviors_tests, fixture)

CAF_TEST(composition) {
  CAF_MESSAGE("test foo_actor_state");
  auto f1 = sys.spawn<foo_actor_state>();
  inject((int, int, int), from(self).to(f1).with(1, 2, 4));
  expect((int), from(f1).to(self).with(7));
  inject((double), from(self).to(f1).with(42.0));
  expect((double, double), from(f1).to(self).with(42.0, 42.0));
  CAF_MESSAGE("test composed_behavior<i3_actor_state, d_actor_state>");
  f1 = sys.spawn<composed_behavior<i3_actor_state, d_actor_state>>();
  inject((int, int, int), from(self).to(f1).with(1, 2, 4));
  expect((int), from(f1).to(self).with(7));
  inject((double), from(self).to(f1).with(42.0));
  expect((double, double), from(f1).to(self).with(42.0, 42.0));
  CAF_MESSAGE("test composed_behavior<i3_actor_state2, d_actor_state>");
  f1 = sys.spawn<composed_behavior<i3_actor_state2, d_actor_state>>();
  inject((int, int, int), from(self).to(f1).with(1, 2, 4));
  expect((int), from(f1).to(self).with(8));
  inject((double), from(self).to(f1).with(42.0));
  expect((double, double), from(f1).to(self).with(42.0, 42.0));
  CAF_MESSAGE("test foo_actor_state2");
  f1 = sys.spawn<foo_actor_state2>();
  inject((int, int, int), from(self).to(f1).with(1, 2, 4));
  expect((int), from(f1).to(self).with(-5));
  inject((double), from(self).to(f1).with(42.0));
  expect((double, double), from(f1).to(self).with(42.0, 42.0));
}

CAF_TEST(param_detaching) {
  auto dict = actor_cast<actor>(sys.spawn<dict_state>());
  // Using CAF is the key to success!
  counting_string key = "CAF";
  counting_string value = "success";
  CAF_CHECK_EQUAL(counting_strings_created.load(), 2);
  CAF_CHECK_EQUAL(counting_strings_moved.load(), 0);
  CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 0);
  // Wrap two strings into messages.
  auto put_msg = make_message(put_atom_v, key, value);
  auto get_msg = make_message(get_atom_v, key);
  CAF_CHECK_EQUAL(counting_strings_created.load(), 5);
  CAF_CHECK_EQUAL(counting_strings_moved.load(), 0);
  CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 0);
  // Send put message to dictionary.
  self->send(dict, put_msg);
  sched.run();
  // The handler of put_atom calls .move() on key and value, both causing to
  // detach + move into the map.
  CAF_CHECK_EQUAL(counting_strings_created.load(), 9);
  CAF_CHECK_EQUAL(counting_strings_moved.load(), 2);
  CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 2);
  // Send put message to dictionary again.
  self->send(dict, put_msg);
  sched.run();
  // The handler checks whether key already exists -> no copies.
  CAF_CHECK_EQUAL(counting_strings_created.load(), 9);
  CAF_CHECK_EQUAL(counting_strings_moved.load(), 2);
  CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 2);
  // Alter our initial put, this time moving it to the dictionary.
  put_msg.get_mutable_as<counting_string>(1) = "neverlord";
  put_msg.get_mutable_as<counting_string>(2) = "CAF";
  // Send new put message to dictionary.
  self->send(dict, std::move(put_msg));
  sched.run();
  // The handler of put_atom calls .move() on key and value, but no detaching
  // occurs this time (unique access) -> move into the map.
  CAF_CHECK_EQUAL(counting_strings_created.load(), 11);
  CAF_CHECK_EQUAL(counting_strings_moved.load(), 4);
  CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 4);
  // Finally, check for original key.
  self->send(dict, std::move(get_msg));
  sched.run();
  self->receive([&](const counting_string& str) {
    // We receive a copy of the value, which is copied out of the map and then
    // moved into the result message; the string from our get_msg is destroyed.
    CAF_CHECK_EQUAL(counting_strings_created.load(), 13);
    CAF_CHECK_EQUAL(counting_strings_moved.load(), 5);
    CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 6);
    CAF_CHECK_EQUAL(str, "success");
  });
  // Temporary of our handler is destroyed.
  CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 7);
  self->send_exit(dict, exit_reason::user_shutdown);
  sched.run();
  dict = nullptr;
  // Only `key` and `value` from this scope remain.
  CAF_CHECK_EQUAL(counting_strings_destroyed.load(), 11);
}

CAF_TEST(delayed_sends) {
  auto testee = self->spawn<delayed_testee>();
  inject((int), from(self).to(testee).with(42));
  disallow((bool), from(_).to(testee));
  sched.trigger_timeouts();
  expect((bool), from(_).to(testee));
  disallow((std::string), from(testee).to(testee).with("hello"));
  sched.trigger_timeouts();
  expect((std::string), from(testee).to(testee).with("hello"));
}

CAF_TEST(dynamic_spawning) {
  auto testee = unbox(sys.spawn<foo_actor>("foo_actor", make_message()));
  inject((int, int, int), from(self).to(testee).with(1, 2, 4));
  expect((int), from(testee).to(self).with(7));
  inject((double), from(self).to(testee).with(42.0));
  expect((double, double), from(testee).to(self).with(42.0, 42.0));
}

CAF_TEST(streaming) {
  auto src = sys.spawn<source_actor_state>();
  auto stg = sys.spawn<stage_actor_state>();
  auto snk = sys.spawn<sink_actor_state>();
  using src_to_stg = typed_actor<replies_to<open_atom>::with<stream<int>>>;
  using stg_to_snk = typed_actor<reacts_to<stream<int>>>;
  static_assert(std::is_same<decltype(stg * src), src_to_stg>::value,
                "stg * src produces the wrong type");
  static_assert(std::is_same<decltype(snk * stg), stg_to_snk>::value,
                "stg * src produces the wrong type");
  auto pipeline = snk * stg * src;
  self->send(pipeline, open_atom_v);
  run();
  using sink_actor = composable_behavior_based_actor<sink_actor_state>;
  auto& st = deref<sink_actor>(snk).state;
  CAF_CHECK_EQUAL(st.buf.size(), 50u);
  auto is_even = [](int x) { return x % 2 == 0; };
  CAF_CHECK(std::all_of(st.buf.begin(), st.buf.end(), is_even));
  anon_send_exit(src, exit_reason::user_shutdown);
  anon_send_exit(stg, exit_reason::user_shutdown);
  anon_send_exit(snk, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
