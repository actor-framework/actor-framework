// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE config_value_writer

#include "caf/config_value_writer.hpp"

#include "core-test.hpp"

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
  config_value x;
  settings dummy;

  template <class T>
  void set(const T& value) {
    config_value_writer writer{&x};
    if (!detail::save(writer, value))
      CAF_FAIL("failed two write to settings: " << writer.get_error());
  }

  const settings& xs() const {
    if (auto* ptr = get_if<settings>(&x))
      return *ptr;
    else
      return dummy;
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(structs become dictionaries) {
  set(foobar{"hello", "world"});
  CHECK_EQ(get_as<std::string>(xs(), "foo"), "hello"s);
  CHECK_EQ(get_as<std::string>(xs(), "bar"), "world"s);
}

CAF_TEST(nested structs become nested dictionaries) {
  set(line{{10, 20, 30}, {70, 60, 50}});
  CHECK_EQ(get_as<i64>(xs(), "p1.x"), 10_i64);
  CHECK_EQ(get_as<i64>(xs(), "p1.y"), 20_i64);
  CHECK_EQ(get_as<i64>(xs(), "p1.z"), 30_i64);
  CHECK_EQ(get_as<i64>(xs(), "p2.x"), 70_i64);
  CHECK_EQ(get_as<i64>(xs(), "p2.y"), 60_i64);
  CHECK_EQ(get_as<i64>(xs(), "p2.z"), 50_i64);
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
  CHECK_EQ(get_as<settings>(xs(), "v1"), settings{});
  CHECK_EQ(get_as<i64>(xs(), "v2"), 42_i64);
  CHECK_EQ(get_as<i64_list>(xs(), "v3"), i64_list({-1, -2, -3, -4}));
  if (auto v4 = get_as<config_value::list>(xs(), "v4");
      CHECK(v4 && v4->size() == 2u)) {
    if (auto v1 = v4->front(); CHECK(holds_alternative<settings>(v1))) {
      auto& v1_xs = get<settings>(v1);
      CHECK_EQ(get<double>(v1_xs, "content"), 0.0);
      CHECK_EQ(get<std::string>(v1_xs, "@content-type"),
               to_string(type_name_v<double>));
    }
    if (auto v2 = v4->back(); CHECK(holds_alternative<settings>(v2))) {
      auto& v2_xs = get<settings>(v2);
      CHECK_EQ(get<double>(v2_xs, "content"), 1.0);
      CHECK_EQ(get<std::string>(v2_xs, "@content-type"),
               to_string(type_name_v<double>));
    }
  }
  CHECK_EQ(get_as<i64_list>(xs(), "v5"), i64_list({10, 20}));
  // TODO: check v6
  CHECK_EQ(get_as<i64>(xs(), "v7.one"), 1_i64);
  CHECK_EQ(get_as<i64>(xs(), "v7.two"), 2_i64);
  CHECK_EQ(get_as<i64>(xs(), "v7.three"), 3_i64);
  CHECK_EQ(get_as<config_value::list>(xs(), "v8"), config_value::list());
}

CAF_TEST(custom inspect overloads may produce single values) {
  auto tue = weekday::tuesday;
  set(tue);
  CHECK_EQ(get_as<std::string>(x), "tuesday"s);
}

END_FIXTURE_SCOPE()
