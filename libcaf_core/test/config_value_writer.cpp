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

#define CAF_SUITE config_value_writer

#include "caf/config_value_writer.hpp"

#include "caf/test/dsl.hpp"

#include "inspector-tests.hpp"

#include "caf/inspector_access.hpp"

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
  void set(const T& value) {
    config_value val;
    config_value_writer writer{&val};
    if (!detail::save_value(writer, value))
      CAF_FAIL("failed two write to settings: " << writer.get_error());
    if (!holds_alternative<settings>(val))
      CAF_FAIL("serializing T did not result in a dictionary");
    xs = std::move(caf::get<settings>(val));
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

CAF_TEST_FIXTURE_SCOPE(config_value_writer_tests, fixture)

CAF_TEST(structs become dictionaries) {
  set(foobar{"hello", "world"});
  CAF_CHECK_EQUAL(get<std::string>("foo"), "hello"s);
  CAF_CHECK_EQUAL(get<std::string>("bar"), "world"s);
}

CAF_TEST(nested structs become nested dictionaries) {
  set(line{{10, 20, 30}, {70, 60, 50}});
  CAF_CHECK_EQUAL(get<i64>("p1.x"), 10_i64);
  CAF_CHECK_EQUAL(get<i64>("p1.y"), 20_i64);
  CAF_CHECK_EQUAL(get<i64>("p1.z"), 30_i64);
  CAF_CHECK_EQUAL(get<i64>("p2.x"), 70_i64);
  CAF_CHECK_EQUAL(get<i64>("p2.y"), 60_i64);
  CAF_CHECK_EQUAL(get<i64>("p2.z"), 50_i64);
}

CAF_TEST(empty types and maps become dictionaries) {
  basics tst;
  tst.v2 = 42;
  for (size_t index = 0; index < 4; ++index)
    tst.v3[index] = -static_cast<int32_t>(index + 1);
  for (size_t index = 0; index < 2; ++index)
    tst.v4[index] = dummy_message{{static_cast<double>(index)}};
  for (size_t index = 0; index < 2; ++index)
    tst.v5[index] = static_cast<int32_t>(index + 1) * 10;
  tst.v6 = std::make_tuple(42, dummy_message{{"foobar"s}});
  tst.v7["one"] = 1;
  tst.v7["two"] = 2;
  tst.v7["three"] = 3;
  set(tst);
  CAF_MESSAGE(xs);
  CAF_CHECK_EQUAL(get<settings>("v1"), settings{});
  CAF_CHECK_EQUAL(get<i64>("v2"), 42_i64);
  CAF_CHECK_EQUAL(get<i64_list>("v3"), i64_list({-1, -2, -3, -4}));
  if (auto v4 = get<config_value::list>("v4");
      CAF_CHECK_EQUAL(v4->size(), 2u)) {
    if (auto v1 = v4->front(); CAF_CHECK(holds_alternative<settings>(v1))) {
      auto& v1_xs = caf::get<settings>(v1);
      CAF_CHECK_EQUAL(get<double>(v1_xs, "content"), 0.0);
      CAF_CHECK_EQUAL(get<std::string>(v1_xs, "@content-type"),
                      to_string(type_name_v<double>));
    }
    if (auto v2 = v4->back(); CAF_CHECK(holds_alternative<settings>(v2))) {
      auto& v2_xs = caf::get<settings>(v2);
      CAF_CHECK_EQUAL(get<double>(v2_xs, "content"), 1.0);
      CAF_CHECK_EQUAL(get<std::string>(v2_xs, "@content-type"),
                      to_string(type_name_v<double>));
    }
  }
  CAF_CHECK_EQUAL(get<i64_list>("v5"), i64_list({10, 20}));
  // TODO: check v6
  CAF_CHECK_EQUAL(get<i64>("v7.one"), 1_i64);
  CAF_CHECK_EQUAL(get<i64>("v7.two"), 2_i64);
  CAF_CHECK_EQUAL(get<i64>("v7.three"), 3_i64);
  CAF_CHECK_EQUAL(get<config_value::list>("v8"), config_value::list());
}

CAF_TEST_FIXTURE_SCOPE_END()
