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

#define CAF_SUITE serialization

#include "caf/test/dsl.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <memory>
#include <new>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/get_mac_addresses.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/make_type_erased_tuple_view.hpp"
#include "caf/make_type_erased_view.hpp"
#include "caf/message.hpp"
#include "caf/message_handler.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/ref_counted.hpp"
#include "caf/serializer.hpp"
#include "caf/variant.hpp"

using namespace std;
using namespace caf;
using caf::detail::type_erased_value_impl;

namespace {

using strmap = map<string, u16string>;

struct raw_struct {
  string str;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, raw_struct& x) {
  return f(x.str);
}

bool operator==(const raw_struct& lhs, const raw_struct& rhs) {
  return lhs.str == rhs.str;
}

enum class test_enum : uint32_t {
  a,
  b,
  c,
};

const char* test_enum_strings[] = {
  "a",
  "b",
  "c",
};

std::string to_string(test_enum x) {
  return test_enum_strings[static_cast<uint32_t>(x)];
}

struct test_array {
  int value[4];
  int value2[2][4];
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, test_array& x) {
  return f(x.value, x.value2);
}

struct test_empty_non_pod {
  test_empty_non_pod() = default;
  test_empty_non_pod(const test_empty_non_pod&) = default;
  test_empty_non_pod& operator=(const test_empty_non_pod&) = default;
  virtual void foo() {
    // nop
  }
  virtual ~test_empty_non_pod() {
    // nop
  }
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, test_empty_non_pod&) {
  return f();
}

class config : public actor_system_config {
public:
  config() {
    add_message_type<test_enum>("test_enum");
    add_message_type<raw_struct>("raw_struct");
    add_message_type<test_array>("test_array");
    add_message_type<test_empty_non_pod>("test_empty_non_pod");
    add_message_type<std::vector<bool>>("bool_vector");
  }
};

struct fixture : test_coordinator_fixture<config> {
  int32_t i32 = -345;
  int64_t i64 = -1234567890123456789ll;
  float f32 = 3.45f;
  double f64 = 54.3;
  timestamp ts = timestamp{timestamp::duration{1478715821 * 1000000000ll}};
  test_enum te = test_enum::b;
  string str = "Lorem ipsum dolor sit amet.";
  raw_struct rs;
  test_array ta{
    {0, 1, 2, 3},
    {{0, 1, 2, 3}, {4, 5, 6, 7}},
  };
  int ra[3] = {1, 2, 3};

  message msg;
  message recursive;

  template <class... Ts>
  byte_buffer serialize(const Ts&... xs) {
    byte_buffer buf;
    binary_serializer sink{sys, buf};
    if (auto err = sink(xs...))
      CAF_FAIL("serialization failed: "
               << sys.render(err)
               << ", data: " << deep_to_string(std::forward_as_tuple(xs...)));
    return buf;
  }

  template <class... Ts>
  void deserialize(const byte_buffer& buf, Ts&... xs) {
    binary_deserializer source{sys, buf};
    if (auto err = source(xs...))
      CAF_FAIL("deserialization failed: " << sys.render(err));
  }

  // serializes `x` and then deserializes and returns the serialized value
  template <class T>
  T roundtrip(T x) {
    T result;
    deserialize(serialize(x), result);
    return result;
  }

  // converts `x` to a message, serialize it, then deserializes it, and
  // finally returns unboxed value
  template <class T>
  T msg_roundtrip(const T& x) {
    message result;
    auto tmp = make_message(x);
    deserialize(serialize(tmp), result);
    if (!result.match_elements<T>())
      CAF_FAIL("expected: " << x << ", got: " << result);
    return result.get_as<T>(0);
  }

  fixture() {
    rs.str.assign(string(str.rbegin(), str.rend()));
    msg = make_message(i32, i64, ts, te, str, rs);
    config_value::dictionary dict;
    put(dict, "scheduler.policy", "none");
    put(dict, "scheduler.max-threads", 42);
    put(dict, "nodes.preload",
        make_config_value_list("sun", "venus", "mercury", "earth", "mars"));
    recursive = make_message(config_value{std::move(dict)});
  }
};

struct is_message {
  explicit is_message(message& msgref) : msg(msgref) {
    // nop
  }

  message& msg;

  template <class T, class... Ts>
  bool equal(T&& v, Ts&&... vs) {
    bool ok = false;
    // work around for gcc 4.8.4 bug
    auto tup = tie(v, vs...);
    message_handler impl{
      [&](T const& u, Ts const&... us) { ok = tup == tie(u, us...); }};
    impl(msg);
    return ok;
  }
};

} // namespace

#define CHECK_RT(val) CAF_CHECK_EQUAL(val, roundtrip(val))

#define CHECK_MSG_RT(val) CAF_CHECK_EQUAL(val, msg_roundtrip(val))

CAF_TEST_FIXTURE_SCOPE(serialization_tests, fixture)

CAF_TEST(ieee_754_conversion) {
  // check conversion of float
  float f1 = 3.1415925f;              // float value
  auto p1 = caf::detail::pack754(f1); // packet value
  CAF_CHECK_EQUAL(p1, static_cast<decltype(p1)>(0x40490FDA));
  auto u1 = caf::detail::unpack754(p1); // unpacked value
  CAF_CHECK_EQUAL(f1, u1);
  // check conversion of double
  double f2 = 3.14159265358979311600; // double value
  auto p2 = caf::detail::pack754(f2); // packet value
  CAF_CHECK_EQUAL(p2, static_cast<decltype(p2)>(0x400921FB54442D18));
  auto u2 = caf::detail::unpack754(p2); // unpacked value
  CAF_CHECK_EQUAL(f2, u2);
}

CAF_TEST(serializing and then deserializing produces the same value) {
  CHECK_RT(i32);
  CHECK_RT(i64);
  CHECK_RT(f32);
  CHECK_RT(f64);
  CHECK_RT(ts);
  CHECK_RT(te);
  CHECK_RT(str);
  CHECK_RT(rs);
}

CAF_TEST(messages serialize and deserialize their content) {
  CHECK_MSG_RT(i32);
  CHECK_MSG_RT(i64);
  CHECK_MSG_RT(f32);
  CHECK_MSG_RT(f64);
  CHECK_MSG_RT(ts);
  CHECK_MSG_RT(te);
  CHECK_MSG_RT(str);
  CHECK_MSG_RT(rs);
}

CAF_TEST(raw_arrays) {
  auto buf = serialize(ra);
  int x[3];
  deserialize(buf, x);
  for (auto i = 0; i < 3; ++i)
    CAF_CHECK_EQUAL(ra[i], x[i]);
}

CAF_TEST(arrays) {
  auto buf = serialize(ta);
  test_array x;
  deserialize(buf, x);
  for (auto i = 0; i < 4; ++i)
    CAF_CHECK_EQUAL(ta.value[i], x.value[i]);
  for (auto i = 0; i < 2; ++i)
    for (auto j = 0; j < 4; ++j)
      CAF_CHECK_EQUAL(ta.value2[i][j], x.value2[i][j]);
}

CAF_TEST(empty_non_pods) {
  test_empty_non_pod x;
  auto buf = serialize(x);
  CAF_REQUIRE(buf.empty());
  deserialize(buf, x);
}

std::string hexstr(const std::vector<char>& buf) {
  using namespace std;
  ostringstream oss;
  oss << hex;
  oss.fill('0');
  for (auto& c : buf) {
    oss.width(2);
    oss << int{c};
  }
  return oss.str();
}

CAF_TEST(messages) {
  // serialize original message which uses tuple_vals internally and
  // deserialize into a message which uses type_erased_value pointers
  message x;
  auto buf1 = serialize(msg);
  deserialize(buf1, x);
  CAF_CHECK_EQUAL(to_string(msg), to_string(x));
  CAF_CHECK(is_message(x).equal(i32, i64, ts, te, str, rs));
  // serialize fully dynamic message again (do another roundtrip)
  message y;
  auto buf2 = serialize(x);
  CAF_CHECK_EQUAL(buf1, buf2);
  deserialize(buf2, y);
  CAF_CHECK_EQUAL(to_string(msg), to_string(y));
  CAF_CHECK(is_message(y).equal(i32, i64, ts, te, str, rs));
  CAF_CHECK_EQUAL(to_string(recursive), to_string(roundtrip(recursive)));
}

CAF_TEST(multiple_messages) {
  auto m = make_message(rs, te);
  auto buf = serialize(te, m, msg);
  test_enum t;
  message m1;
  message m2;
  deserialize(buf, t, m1, m2);
  CAF_CHECK_EQUAL(std::make_tuple(t, to_string(m1), to_string(m2)),
                  std::make_tuple(te, to_string(m), to_string(msg)));
  CAF_CHECK(is_message(m1).equal(rs, te));
  CAF_CHECK(is_message(m2).equal(i32, i64, ts, te, str, rs));
}

CAF_TEST(type_erased_value) {
  auto buf = serialize(str);
  type_erased_value_ptr ptr{new type_erased_value_impl<std::string>};
  binary_deserializer source{sys, buf};
  ptr->load(source);
  CAF_CHECK_EQUAL(str, *reinterpret_cast<const std::string*>(ptr->get()));
}

CAF_TEST(type_erased_view) {
  auto str_view = make_type_erased_view(str);
  auto buf = serialize(str_view);
  std::string res;
  deserialize(buf, res);
  CAF_CHECK_EQUAL(str, res);
}

CAF_TEST(type_erased_tuple) {
  auto tview = make_type_erased_tuple_view(str, i32);
  CAF_CHECK_EQUAL(to_string(tview), deep_to_string(std::make_tuple(str, i32)));
  auto buf = serialize(tview);
  CAF_REQUIRE(!buf.empty());
  std::string tmp1;
  int32_t tmp2;
  deserialize(buf, tmp1, tmp2);
  CAF_CHECK_EQUAL(tmp1, str);
  CAF_CHECK_EQUAL(tmp2, i32);
  deserialize(buf, tview);
  CAF_CHECK_EQUAL(to_string(tview), deep_to_string(std::make_tuple(str, i32)));
}

CAF_TEST(long_sequences) {
  byte_buffer data;
  binary_serializer sink{nullptr, data};
  size_t n = std::numeric_limits<uint32_t>::max();
  sink.begin_sequence(n);
  sink.end_sequence();
  binary_deserializer source{nullptr, data};
  size_t m = 0;
  source.begin_sequence(m);
  source.end_sequence();
  CAF_CHECK_EQUAL(n, m);
}

CAF_TEST(non_empty_vector) {
  CAF_MESSAGE("deserializing into a non-empty vector overrides any content");
  std::vector<int> foo{1, 2, 3};
  std::vector<int> bar{0};
  auto buf = serialize(foo);
  deserialize(buf, bar);
  CAF_CHECK_EQUAL(foo, bar);
}

CAF_TEST(variant_with_tree_types) {
  CAF_MESSAGE("deserializing into a non-empty vector overrides any content");
  using test_variant = variant<int, double, std::string>;
  test_variant x{42};
  CAF_CHECK_EQUAL(x, roundtrip(x));
  x = 12.34;
  CAF_CHECK_EQUAL(x, roundtrip(x));
  x = std::string{"foobar"};
  CAF_CHECK_EQUAL(x, roundtrip(x));
}

// -- our vector<bool> serialization packs into an uint64_t. Hence, the
// critical sizes to test are 0, 1, 63, 64, and 65.

CAF_TEST(bool_vector_size_0) {
  std::vector<bool> xs;
  CAF_CHECK_EQUAL(deep_to_string(xs), "[]");
  CAF_CHECK_EQUAL(xs, roundtrip(xs));
  CAF_CHECK_EQUAL(xs, msg_roundtrip(xs));
}

CAF_TEST(bool_vector_size_1) {
  std::vector<bool> xs{true};
  CAF_CHECK_EQUAL(deep_to_string(xs), "[true]");
  CAF_CHECK_EQUAL(xs, roundtrip(xs));
  CAF_CHECK_EQUAL(xs, msg_roundtrip(xs));
}

CAF_TEST(bool_vector_size_2) {
  std::vector<bool> xs{true, true};
  CAF_CHECK_EQUAL(deep_to_string(xs), "[true, true]");
  CAF_CHECK_EQUAL(xs, roundtrip(xs));
  CAF_CHECK_EQUAL(xs, msg_roundtrip(xs));
}

CAF_TEST(bool_vector_size_63) {
  std::vector<bool> xs;
  for (int i = 0; i < 63; ++i)
    xs.push_back(i % 3 == 0);
  CAF_CHECK_EQUAL(
    deep_to_string(xs),
    "[true, false, false, true, false, false, true, false, false, true, false, "
    "false, true, false, false, true, false, false, true, false, false, true, "
    "false, false, true, false, false, true, false, false, true, false, false, "
    "true, false, false, true, false, false, true, false, false, true, false, "
    "false, true, false, false, true, false, false, true, false, false, true, "
    "false, false, true, false, false, true, false, false]");
  CAF_CHECK_EQUAL(xs, roundtrip(xs));
  CAF_CHECK_EQUAL(xs, msg_roundtrip(xs));
}

CAF_TEST(bool_vector_size_64) {
  std::vector<bool> xs;
  for (int i = 0; i < 64; ++i)
    xs.push_back(i % 5 == 0);
  CAF_CHECK_EQUAL(deep_to_string(xs),
                  "[true, false, false, false, false, true, false, false, "
                  "false, false, true, false, false, false, false, true, "
                  "false, false, false, false, true, false, false, false, "
                  "false, true, false, false, false, false, true, false, "
                  "false, false, false, true, false, false, false, false, "
                  "true, false, false, false, false, true, false, false, "
                  "false, false, true, false, false, false, false, true, "
                  "false, false, false, false, true, false, false, false]");
  CAF_CHECK_EQUAL(xs, roundtrip(xs));
  CAF_CHECK_EQUAL(xs, msg_roundtrip(xs));
}

CAF_TEST(bool_vector_size_65) {
  std::vector<bool> xs;
  for (int i = 0; i < 65; ++i)
    xs.push_back(!(i % 7 == 0));
  CAF_CHECK_EQUAL(
    deep_to_string(xs),
    "[false, true, true, true, true, true, true, false, true, true, true, "
    "true, true, true, false, true, true, true, true, true, true, false, true, "
    "true, true, true, true, true, false, true, true, true, true, true, true, "
    "false, true, true, true, true, true, true, false, true, true, true, true, "
    "true, true, false, true, true, true, true, true, true, false, true, true, "
    "true, true, true, true, false, true]");
  CAF_CHECK_EQUAL(xs, roundtrip(xs));
  CAF_CHECK_EQUAL(xs, msg_roundtrip(xs));
}

CAF_TEST_FIXTURE_SCOPE_END()
