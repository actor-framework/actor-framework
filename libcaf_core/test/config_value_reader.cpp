// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE config_value_reader

#include "caf/config_value_reader.hpp"

#include "core-test.hpp"

#include "inspector-tests.hpp"

#include "caf/config_value.hpp"
#include "caf/config_value_writer.hpp"
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

  template <class T>
  void deserialize(const config_value& src, T& value) {
    config_value_reader reader{&src};
    if (!detail::load(reader, value))
      CAF_FAIL("deserialization failed: " << reader.get_error());
  }

  template <class T>
  void deserialize(const settings& src, T& value) {
    deserialize(config_value{src}, value);
  }

  template <class T>
  void deserialize(T& value) {
    return deserialize(x, value);
  }

  template <class T>
  std::optional<T> get(const settings& cfg, std::string_view key) {
    if (auto ptr = get_if<T>(&cfg, key))
      return *ptr;
    else
      return {};
  }

  template <class T>
  std::optional<T> get(std::string_view key) {
    if (auto* xs = get_if<settings>(&x))
      return get<T>(*xs, key);
    else
      CAF_FAIL("fixture does not contain a dictionary");
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(readers deserialize builtin types from config values) {
  std::string value;
  auto& xs = x.as_dictionary();
  put(xs, "foo", "bar");
  deserialize(xs["foo"], value);
  CHECK_EQ(value, "bar");
}

CAF_TEST(readers deserialize simple objects from configs) {
  auto& xs = x.as_dictionary();
  put(xs, "foo", "hello");
  put(xs, "bar", "world");
  foobar fb;
  deserialize(fb);
  CHECK_EQ(fb.foo(), "hello"s);
  CHECK_EQ(fb.bar(), "world"s);
}

CAF_TEST(readers deserialize complex objects from configs) {
  MESSAGE("fill a dictionary with data for a 'basics' object");
  auto& xs = x.as_dictionary();
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
  MESSAGE("deserialize and verify the 'basics' object");
  basics obj;
  deserialize(obj);
  CHECK_EQ(obj.v2, 42);
  CHECK_EQ(obj.v3[0], 1);
  CHECK_EQ(obj.v3[1], 2);
  CHECK_EQ(obj.v3[2], 3);
  CHECK_EQ(obj.v3[3], 4);
  CHECK_EQ(obj.v4[0], dummy_message{{2.0}});
  CHECK_EQ(obj.v4[1], dummy_message{{"foobar"s}});
  CHECK_EQ(obj.v5[0], i64{10});
  CHECK_EQ(obj.v5[1], i64{20});
  CHECK_EQ(obj.v6, std::make_tuple(int32_t{123}, dummy_message{{2.0}}));
  CHECK_EQ(obj.v7["one"], 1);
  CHECK_EQ(obj.v7["two"], 2);
  CHECK_EQ(obj.v7["three"], 3);
}

CAF_TEST(readers deserialize objects from the output of writers) {
  MESSAGE("serialize the 'line' object");
  {
    line l{{10, 20, 30}, {70, 60, 50}};
    config_value tmp;
    config_value_writer writer{&tmp};
    if (!detail::save(writer, l))
      CAF_FAIL("failed two write to settings: " << writer.get_error());
    if (!holds_alternative<settings>(tmp))
      CAF_FAIL("writer failed to produce a dictionary");
    x.as_dictionary() = std::move(caf::get<settings>(tmp));
  }
  MESSAGE("serialize and verify the 'line' object");
  {
    line l{{0, 0, 0}, {0, 0, 0}};
    deserialize(l);
    CHECK_EQ(l.p1.x, 10);
    CHECK_EQ(l.p1.y, 20);
    CHECK_EQ(l.p1.z, 30);
    CHECK_EQ(l.p2.x, 70);
    CHECK_EQ(l.p2.y, 60);
    CHECK_EQ(l.p2.z, 50);
  }
}

END_FIXTURE_SCOPE()
