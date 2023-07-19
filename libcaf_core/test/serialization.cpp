// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE serialization

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/stringification_inspector.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/json_reader.hpp"
#include "caf/json_writer.hpp"
#include "caf/message.hpp"
#include "caf/message_handler.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/ref_counted.hpp"
#include "caf/sec.hpp"
#include "caf/serializer.hpp"

#include "core-test.hpp"

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
#include <map>
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

struct opaque {
  int secret;
};

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(opaque)

struct the_great_unknown {
  int secret;
};

using namespace caf;

using bs = binary_serializer;

using si = detail::stringification_inspector;

using iat = inspector_access_type;

template <class Inspector, class T>
struct access_of {
  template <class What>
  static constexpr bool is
    = std::is_same<decltype(inspect_access_type<Inspector, T>()), What>::value;
};

static_assert(
  access_of<bs, std::variant<int, double>>::is<iat::specialization>);

static_assert(access_of<bs, sec>::is<iat::inspect>);

static_assert(access_of<bs, int>::is<iat::builtin>);

static_assert(access_of<bs, dummy_tag_type>::is<iat::empty>);

static_assert(access_of<bs, opaque>::is<iat::unsafe>);

static_assert(access_of<bs, std::tuple<int, double>>::is<iat::tuple>);

static_assert(access_of<bs, std::map<int, int>>::is<iat::map>);

static_assert(access_of<bs, std::vector<bool>>::is<iat::list>);

static_assert(access_of<bs, the_great_unknown>::is<iat::none>);

// The stringification inspector picks up to_string via builtin_inspect.

static_assert(access_of<si, sec>::is<iat::builtin_inspect>);

static_assert(access_of<si, timespan>::is<iat::builtin_inspect>);

const char* test_enum_strings[] = {
  "a",
  "b",
  "c",
};

std::string to_string(test_enum x) {
  return test_enum_strings[static_cast<uint32_t>(x)];
}

void test_empty_non_pod::foo() {
  // nop
}

test_empty_non_pod::~test_empty_non_pod() {
  // nop
}

namespace {

struct fixture : test_coordinator_fixture<> {
  int32_t i32 = -345;
  int64_t i64 = -1234567890123456789ll;
  float f32 = 3.45f;
  float f32_nan = std::numeric_limits<float>::quiet_NaN();
  float f32_pos_inf = std::numeric_limits<float>::infinity();
  float f32_neg_inf = -std::numeric_limits<float>::infinity();
  double f64 = 54.3;
  double f64_nan = std::numeric_limits<double>::quiet_NaN();
  double f64_pos_inf = std::numeric_limits<double>::infinity();
  double f64_neg_inf = -std::numeric_limits<double>::infinity();
  timestamp ts = timestamp{timestamp::duration{1478715821 * 1000000000ll}};
  test_enum te = test_enum::b;
  std::string str = "Lorem ipsum dolor sit amet.";
  raw_struct rs;
  test_array ta{
    {0, 1, 2, 3},
    {{0, 1, 2, 3}, {4, 5, 6, 7}},
  };
  int ra[3] = {1, 2, 3};

  message msg;
  message recursive;

  json_writer jwriter;

  json_reader jreader;

  template <class... Ts>
  byte_buffer serialize(const Ts&... xs) {
    byte_buffer buf;
    binary_serializer sink{sys, buf};
    if (!(sink.apply(xs) && ...))
      CAF_FAIL("serialization failed: "
               << sink.get_error()
               << ", data: " << deep_to_string(std::forward_as_tuple(xs...)));
    return buf;
  }

  template <class... Ts>
  void deserialize(const byte_buffer& buf, Ts&... xs) {
    binary_deserializer source{sys, buf};
    if (!(source.apply(xs) && ...))
      CAF_FAIL("deserialization failed: " << source.get_error());
  }

  template <class... Ts>
  std::string serialize_json(const Ts&... xs) {
    jwriter.reset();
    if (!(jwriter.apply(xs) && ...))
      CAF_FAIL("JSON serialization failed: "
               << jwriter.get_error()
               << ", data: " << deep_to_string(std::forward_as_tuple(xs...)));
    auto str = jwriter.str();
    return std::string{str.begin(), str.end()};
  }

  template <class... Ts>
  void deserialize_json(const std::string& str, Ts&... xs) {
    if (!jreader.load(str))
      CAF_FAIL("JSON loading failed: " << jreader.get_error()
                                       << "\n     input: " << str);
    if (!(jreader.apply(xs) && ...))
      CAF_FAIL("JSON deserialization failed: " << jreader.get_error()
                                               << "\n     input: " << str);
  }

  // serializes `x` and then deserializes and returns the serialized value
  template <class T>
  T roundtrip(T x, bool enable_json = true) {
    auto r1 = T{};
    deserialize(serialize(x), r1);
    if (enable_json) {
      auto r2 = T{};
      deserialize_json(serialize_json(x), r2);
      if (!CHECK_EQ(r1, r2))
        MESSAGE("generated JSON: " << serialize_json(x));
    }
    return r1;
  }

  // converts `x` to a message, serialize it, then deserializes it, and
  // finally returns unboxed value
  template <class T>
  T msg_roundtrip(const T& x) {
    message result;
    auto tmp = make_message(x);
    auto buf = serialize(tmp);
    MESSAGE("serialized " << to_string(tmp) << " into " << buf.size()
                          << " bytes");
    deserialize(buf, result);
    if (!result.match_elements<T>())
      CAF_FAIL("expected: " << x << ", got: " << result);
    return result.get_as<T>(0);
  }

  fixture() : jwriter(sys), jreader(sys) {
    rs.str.assign(std::string(str.rbegin(), str.rend()));
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
    auto tup = std::tie(v, vs...);
    message_handler impl{
      [&](T const& u, Ts const&... us) { ok = tup == std::tie(u, us...); }};
    impl(msg);
    return ok;
  }
};

} // namespace

#define CHECK_RT(val)                                                          \
  do {                                                                         \
    MESSAGE(#val);                                                             \
    CHECK_EQ(val, roundtrip(val));                                             \
  } while (false)

#define CHECK_PRED_RT(pred, value)                                             \
  do {                                                                         \
    MESSAGE(#pred "(" #value ")");                                             \
    CHECK(pred(roundtrip(value, false)));                                      \
  } while (false)

#define CHECK_SIGN_RT(value)                                                   \
  CHECK_EQ(std::signbit(roundtrip(value, false)), std::signbit(value))

#define CHECK_MSG_RT(val) CHECK_EQ(val, msg_roundtrip(val))

#define CHECK_PRED_MSG_RT(pred, value) CHECK(pred(msg_roundtrip(value)))

#define CHECK_SIGN_MSG_RT(value)                                               \
  CHECK_EQ(std::signbit(msg_roundtrip(value)), std::signbit(value))

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(serializing and then deserializing produces the same value) {
  CHECK_RT(i32);
  CHECK_RT(i64);
  CHECK_RT(f32);
  CHECK_RT(f64);
  CHECK_RT(ts);
  CHECK_RT(te);
  CHECK_RT(str);
  CHECK_RT(rs);
  CHECK_PRED_RT(std::isnan, f32_nan);
  CHECK_PRED_RT(std::isinf, f32_pos_inf);
  CHECK_PRED_RT(std::isinf, f32_neg_inf);
  CHECK_PRED_RT(std::isnan, f64_nan);
  CHECK_PRED_RT(std::isinf, f64_pos_inf);
  CHECK_PRED_RT(std::isinf, f64_neg_inf);
  CHECK_SIGN_RT(f32_pos_inf);
  CHECK_SIGN_RT(f32_neg_inf);
  CHECK_SIGN_RT(f64_pos_inf);
  CHECK_SIGN_RT(f64_neg_inf);
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
  CHECK_PRED_MSG_RT(std::isnan, f32_nan);
  CHECK_PRED_MSG_RT(std::isinf, f32_pos_inf);
  CHECK_PRED_MSG_RT(std::isinf, f32_neg_inf);
  CHECK_PRED_MSG_RT(std::isnan, f64_nan);
  CHECK_PRED_MSG_RT(std::isinf, f64_pos_inf);
  CHECK_PRED_MSG_RT(std::isinf, f64_neg_inf);
  CHECK_SIGN_MSG_RT(f32_pos_inf);
  CHECK_SIGN_MSG_RT(f32_neg_inf);
  CHECK_SIGN_MSG_RT(f64_pos_inf);
  CHECK_SIGN_MSG_RT(f64_neg_inf);
}

CAF_TEST(raw_arrays) {
  auto buf = serialize(ra);
  int x[3];
  deserialize(buf, x);
  for (auto i = 0; i < 3; ++i)
    CHECK_EQ(ra[i], x[i]);
}

CAF_TEST(arrays) {
  auto buf = serialize(ta);
  test_array x;
  deserialize(buf, x);
  for (auto i = 0; i < 4; ++i)
    CHECK_EQ(ta.value[i], x.value[i]);
  for (auto i = 0; i < 2; ++i)
    for (auto j = 0; j < 4; ++j)
      CHECK_EQ(ta.value2[i][j], x.value2[i][j]);
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
  CHECK_EQ(to_string(msg), to_string(x));
  CHECK(is_message(x).equal(i32, i64, ts, te, str, rs));
  // serialize fully dynamic message again (do another roundtrip)
  message y;
  auto buf2 = serialize(x);
  CHECK_EQ(buf1, buf2);
  deserialize(buf2, y);
  CHECK_EQ(to_string(msg), to_string(y));
  CHECK(is_message(y).equal(i32, i64, ts, te, str, rs));
  CHECK_EQ(to_string(recursive), to_string(roundtrip(recursive, false)));
}

CAF_TEST(multiple_messages) {
  auto m = make_message(rs, te);
  auto buf = serialize(te, m, msg);
  test_enum t;
  message m1;
  message m2;
  deserialize(buf, t, m1, m2);
  CHECK_EQ(std::make_tuple(t, to_string(m1), to_string(m2)),
           std::make_tuple(te, to_string(m), to_string(msg)));
  CHECK(is_message(m1).equal(rs, te));
  CHECK(is_message(m2).equal(i32, i64, ts, te, str, rs));
}

CAF_TEST(long_sequences) {
  byte_buffer data;
  binary_serializer sink{nullptr, data};
  size_t n = std::numeric_limits<uint32_t>::max();
  CHECK(sink.begin_sequence(n));
  CHECK(sink.end_sequence());
  binary_deserializer source{nullptr, data};
  size_t m = 0;
  CHECK(source.begin_sequence(m));
  CHECK(source.end_sequence());
  CHECK_EQ(n, m);
}

CAF_TEST(non_empty_vector) {
  MESSAGE("deserializing into a non-empty vector overrides any content");
  std::vector<int> foo{1, 2, 3};
  std::vector<int> bar{0};
  auto buf = serialize(foo);
  deserialize(buf, bar);
  CHECK_EQ(foo, bar);
}

CAF_TEST(variant_with_tree_types) {
  MESSAGE("deserializing into a non-empty vector overrides any content");
  using test_variant = std::variant<int, double, std::string>;
  auto x = test_variant{42};
  CHECK_EQ(x, roundtrip(x, false));
  x = 12.34;
  CHECK_EQ(x, roundtrip(x, false));
  x = std::string{"foobar"};
  CHECK_EQ(x, roundtrip(x, false));
}

// -- our vector<bool> serialization packs into an uint64_t. Hence, the
// critical sizes to test are 0, 1, 63, 64, and 65.

CAF_TEST(bool_vector_size_0) {
  std::vector<bool> xs;
  CHECK_EQ(deep_to_string(xs), "[]");
  CHECK_EQ(xs, roundtrip(xs));
  CHECK_EQ(xs, msg_roundtrip(xs));
}

CAF_TEST(bool_vector_size_1) {
  std::vector<bool> xs{true};
  CHECK_EQ(deep_to_string(xs), "[true]");
  CHECK_EQ(xs, roundtrip(xs));
  CHECK_EQ(xs, msg_roundtrip(xs));
}

CAF_TEST(bool_vector_size_2) {
  std::vector<bool> xs{true, true};
  CHECK_EQ(deep_to_string(xs), "[true, true]");
  CHECK_EQ(xs, roundtrip(xs));
  CHECK_EQ(xs, msg_roundtrip(xs));
}

CAF_TEST(bool_vector_size_63) {
  std::vector<bool> xs;
  for (int i = 0; i < 63; ++i)
    xs.push_back(i % 3 == 0);
  CHECK_EQ(
    deep_to_string(xs),
    "[true, false, false, true, false, false, true, false, false, true, false, "
    "false, true, false, false, true, false, false, true, false, false, true, "
    "false, false, true, false, false, true, false, false, true, false, false, "
    "true, false, false, true, false, false, true, false, false, true, false, "
    "false, true, false, false, true, false, false, true, false, false, true, "
    "false, false, true, false, false, true, false, false]");
  CHECK_EQ(xs, roundtrip(xs));
  CHECK_EQ(xs, msg_roundtrip(xs));
}

CAF_TEST(bool_vector_size_64) {
  std::vector<bool> xs;
  for (int i = 0; i < 64; ++i)
    xs.push_back(i % 5 == 0);
  CHECK_EQ(deep_to_string(xs),
           "[true, false, false, false, false, true, false, false, "
           "false, false, true, false, false, false, false, true, "
           "false, false, false, false, true, false, false, false, "
           "false, true, false, false, false, false, true, false, "
           "false, false, false, true, false, false, false, false, "
           "true, false, false, false, false, true, false, false, "
           "false, false, true, false, false, false, false, true, "
           "false, false, false, false, true, false, false, false]");
  CHECK_EQ(xs, roundtrip(xs));
  CHECK_EQ(xs, msg_roundtrip(xs));
}

CAF_TEST(bool_vector_size_65) {
  std::vector<bool> xs;
  for (int i = 0; i < 65; ++i)
    xs.push_back(!(i % 7 == 0));
  CHECK_EQ(
    deep_to_string(xs),
    "[false, true, true, true, true, true, true, false, true, true, true, "
    "true, true, true, false, true, true, true, true, true, true, false, true, "
    "true, true, true, true, true, false, true, true, true, true, true, true, "
    "false, true, true, true, true, true, true, false, true, true, true, true, "
    "true, true, false, true, true, true, true, true, true, false, true, true, "
    "true, true, true, true, false, true]");
  CHECK_EQ(xs, roundtrip(xs));
  CHECK_EQ(xs, msg_roundtrip(xs));
}

CAF_TEST(serializers handle actor handles) {
  auto dummy = sys.spawn([]() -> behavior {
    return {
      [](int i) { return i; },
    };
  });
  CHECK_EQ(dummy, roundtrip(dummy, false));
  CHECK_EQ(dummy, msg_roundtrip(dummy));
  std::vector<strong_actor_ptr> wrapped{actor_cast<strong_actor_ptr>(dummy)};
  CHECK_EQ(wrapped, roundtrip(wrapped, false));
  CHECK_EQ(wrapped, msg_roundtrip(wrapped));
  anon_send_exit(dummy, exit_reason::user_shutdown);
}

END_FIXTURE_SCOPE()
