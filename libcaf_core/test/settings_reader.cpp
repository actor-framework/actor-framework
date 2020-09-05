/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE settings_reader

#include "caf/settings_reader.hpp"

#include "caf/test/dsl.hpp"

#include "inspector-tests.hpp"

#include "caf/settings_writer.hpp"

using namespace caf;

using namespace std::literals::string_literals;

namespace {

using i64 = int64_t;

constexpr i64 operator""_i64(unsigned long long int x) {
  return static_cast<int64_t>(x);
}

using i64_list = std::vector<i64>;

struct fixture {
  settings xs;

  template <class T>
  void deserialize(const settings& src, T& value) {
    settings_reader reader{&src};
    if (!inspect_object(reader, value))
      CAF_FAIL("failed to deserialize from settings: " << reader.get_error());
  }

  template <class T>
  void deserialize(T& value) {
    return deserialize(xs, value);
  }

  template <class T>
  optional<T> get(const settings& cfg, string_view key) {
    if (auto ptr = get_if<T>(&cfg, key))
      return *ptr;
    return none;
  }

  template <class T>
  optional<T> get(string_view key) {
    return get<T>(xs, key);
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(settings_reader_tests, fixture)

CAF_TEST(readers deserialize simple objects from configs) {
  put(xs, "foo", "hello");
  put(xs, "bar", "world");
  foobar fb;
  deserialize(fb);
  CAF_CHECK_EQUAL(fb.foo(), "hello"s);
  CAF_CHECK_EQUAL(fb.bar(), "world"s);
}

CAF_TEST(readers deserialize complex objects from configs) {
  CAF_MESSAGE("fill a dictionary with data for a 'basics' object");
  put(xs, "v1", settings{});
  put(xs, "v2", 42_i64);
  put(xs, "v3", i64_list({1, 2, 3, 4}));
  settings msg1;
  put(msg1, "content", 2.0);
  put(msg1, "@content-type", "double");
  settings msg2;
  put(msg2, "content", "foobar"s);
  put(msg2, "@content-type", "std::string");
  put(xs, "v4", make_config_value_list(msg1, msg2));
  put(xs, "v5", i64_list({10, 20}));
  config_value::list v6;
  v6.emplace_back(i64{123});
  v6.emplace_back(msg1);
  put(xs, "v6", v6);
  put(xs, "v7.one", i64{1});
  put(xs, "v7.two", i64{2});
  put(xs, "v7.three", i64{3});
  put(xs, "v8", i64_list());
  CAF_MESSAGE("deserialize and verify the 'basics' object");
  basics obj;
  deserialize(obj);
  CAF_CHECK_EQUAL(obj.v2, 42);
  CAF_CHECK_EQUAL(obj.v3[0], 1);
  CAF_CHECK_EQUAL(obj.v3[1], 2);
  CAF_CHECK_EQUAL(obj.v3[2], 3);
  CAF_CHECK_EQUAL(obj.v3[3], 4);
  CAF_CHECK_EQUAL(obj.v4[0], dummy_message{{2.0}});
  CAF_CHECK_EQUAL(obj.v4[1], dummy_message{{"foobar"s}});
  CAF_CHECK_EQUAL(obj.v5[0], i64{10});
  CAF_CHECK_EQUAL(obj.v5[1], i64{20});
  CAF_CHECK_EQUAL(obj.v6, std::make_tuple(int32_t{123}, dummy_message{{2.0}}));
  CAF_CHECK_EQUAL(obj.v7["one"], 1);
  CAF_CHECK_EQUAL(obj.v7["two"], 2);
  CAF_CHECK_EQUAL(obj.v7["three"], 3);
}

CAF_TEST(readers deserialize objects from the output of writers) {
  CAF_MESSAGE("serialize the 'line' object");
  {
    line l{{10, 20, 30}, {70, 60, 50}};
    settings_writer writer{&xs};
    if (!inspect_object(writer, l))
      CAF_FAIL("failed two write to settings: " << writer.get_error());
  }
  CAF_MESSAGE("serialize and verify the 'line' object");
  {
    line l{{0, 0, 0}, {0, 0, 0}};
    deserialize(l);
    CAF_CHECK_EQUAL(l.p1.x, 10);
    CAF_CHECK_EQUAL(l.p1.y, 20);
    CAF_CHECK_EQUAL(l.p1.z, 30);
    CAF_CHECK_EQUAL(l.p2.x, 70);
    CAF_CHECK_EQUAL(l.p2.y, 60);
    CAF_CHECK_EQUAL(l.p2.z, 50);
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
