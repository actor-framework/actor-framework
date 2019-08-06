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
/*
#include "caf/config.hpp"

#define CAF_SUITE serialization
#include "caf/test/unit_test.hpp"

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
#include "caf/deserializer.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/make_type_erased_tuple_view.hpp"
#include "caf/make_type_erased_view.hpp"
#include "caf/message.hpp"
#include "caf/message_handler.hpp"
#include "caf/primitive_variant.hpp"
#include "caf/proxy_registry.hpp"
#include "caf/ref_counted.hpp"
#include "caf/serializer.hpp"
#include "caf/stream_deserializer.hpp"
#include "caf/stream_serializer.hpp"
#include "caf/streambuf.hpp"
#include "caf/variant.hpp"

#include "caf/detail/enum_to_string.hpp"
#include "caf/detail/get_mac_addresses.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/type_traits.hpp"

using namespace std;
using namespace caf;
using caf::detail::type_erased_value_impl;

namespace {

struct test_data {
  int32_t i32 = -345;
  int64_t i64 = -1234567890123456789ll;
  float f32 = 3.45f;
  double f64 = 54.3;
  duration dur = duration{time_unit::seconds, 123};
  timestamp ts = timestamp{timestamp::duration{1478715821 * 1000000000ll}};
  test_enum te = test_enum::b;
  string str = "Lorem ipsum dolor sit amet.";
  raw_struct rs;
  test_array ta{
    {0, 1, 2, 3},
    {{0, 1, 2, 3}, {4, 5, 6, 7}},
  };
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, test_data& x) {
  return f(x.value, x.value2);
}

template <class Serializer, class Deserializer>
struct fixture {
  int32_t i32 = -345;
  int64_t i64 = -1234567890123456789ll;
  float f32 = 3.45f;
  double f64 = 54.3;
  duration dur = duration{time_unit::seconds, 123};
  timestamp ts = timestamp{timestamp::duration{1478715821 * 1000000000ll}};
  test_enum te = test_enum::b;
  string str = "Lorem ipsum dolor sit amet.";
  raw_struct rs;
  test_array ta{
    {0, 1, 2, 3},
    {{0, 1, 2, 3}, {4, 5, 6, 7}},
  };
  int ra[3] = {1, 2, 3};

  test_data testData{

  };

  config cfg;
  actor_system system;
  message msg;
  message recursive;

  template <class T, class... Ts>
  vector<char> serialize(T& x, Ts&... xs) {
    vector<char> buf;
    binary_serializer sink{system, buf};
    if (auto err = sink(x, xs...))
      CAF_FAIL("serialization failed: "
               << system.render(err) << ", data: "
               << deep_to_string(std::forward_as_tuple(x, xs...)));
    return buf;
  }

  template <class T, class... Ts>
  void deserialize(const vector<char>& buf, T& x, Ts&... xs) {
    binary_deserializer source{system, buf};
    if (auto err = source(x, xs...))
      CAF_FAIL("deserialization failed: " << system.render(err));
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

  fixture() : system(cfg) {
    rs.str.assign(string(str.rbegin(), str.rend()));
    msg = make_message(i32, i64, dur, ts, te, str, rs);
    config_value::dictionary dict;
    put(dict, "scheduler.policy", atom("none"));
    put(dict, "scheduler.max-threads", 42);
    put(dict, "nodes.preload",
        make_config_value_list("sun", "venus", "mercury", "earth", "mars"));
    recursive = make_message(config_value{std::move(dict)});
  }
};

} // namespace

#define SERIALIZATION_TEST(name)                                               \
  template <class Serializer, class Deserializer>                              \
  struct name##_tpl : fixture<Serializer, Deserializer> {                      \
    using super = fixture<Serializer, Deserializer>;                           \
    using super::i32;                                                          \
    using super::i64;                                                          \
    using super::f32;                                                          \
    using super::f64;                                                          \
    using super::dur;                                                          \
    using super::ts;                                                           \
    using super::te;                                                           \
    using super::str;                                                          \
    using super::rs;                                                           \
    using super::ta;                                                           \
    using super::ra;                                                           \
    using super::system;                                                       \
    using super::msg;                                                          \
    using super::recursive;                                                    \
    using super::serialize;                                                    \
    using super::deserialize;                                                  \
    using super::roundtrip;                                                    \
    using super::msg_roundtrip;                                                \
    void run_test_impl();                                                      \
  };                                                                           \
  namespace {                                                                  \
  using name##_binary = name##_tpl<binary_serializer, binary_deserializer>;    \
  using name##_stream = name##_tpl<stream_serializer<vectorbuf>,               \
                                   stream_deserializer<charbuf>>;              \
  ::caf::test::detail::adder<::caf::test::test_impl<name##_binary>>            \
    CAF_UNIQUE(a_binary){CAF_XSTR(CAF_SUITE), CAF_XSTR(name##_binary), false}; \
  ::caf::test::detail::adder<::caf::test::test_impl<name##_stream>>            \
    CAF_UNIQUE(a_stream){CAF_XSTR(CAF_SUITE), CAF_XSTR(name##_stream), false}; \
  }                                                                            \
  template <class Serializer, class Deserializer>                              \
  void name##_tpl<Serializer, Deserializer>::run_test_impl()

SERIALIZATION_TEST(i32_values) {
  auto buf = serialize(i32);
  int32_t x;
  deserialize(buf, x);
  CAF_CHECK_EQUAL(i32, x);
}
*/
