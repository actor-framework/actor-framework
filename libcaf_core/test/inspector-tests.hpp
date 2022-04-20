// This header includes various types for testing save and load inspectors.

#pragma once

#include "caf/type_id.hpp"

#include <optional>
#include <variant>

#include "nasty.hpp"

namespace {

struct point_3d;
struct line;
struct duration;
struct person;
class foobar;
struct dummy_message;
struct fallback_dummy_message;
struct basics;

} // namespace

#define CAF_TEST_SET_TYPE_NAME(type)                                           \
  namespace caf {                                                              \
  template <>                                                                  \
  struct type_name<type> {                                                     \
    static constexpr std::string_view value = #type;                           \
  };                                                                           \
  }

CAF_TEST_SET_TYPE_NAME(point_3d)
CAF_TEST_SET_TYPE_NAME(line)
CAF_TEST_SET_TYPE_NAME(duration)
CAF_TEST_SET_TYPE_NAME(person)
CAF_TEST_SET_TYPE_NAME(foobar)
CAF_TEST_SET_TYPE_NAME(dummy_message)
CAF_TEST_SET_TYPE_NAME(fallback_dummy_message)
CAF_TEST_SET_TYPE_NAME(basics)
CAF_TEST_SET_TYPE_NAME(nasty)

namespace {

struct point_3d {
  int32_t x;
  int32_t y;
  int32_t z;
};

[[maybe_unused]] bool operator==(const point_3d& x, const point_3d& y) {
  return std::tie(x.x, x.y, x.z) == std::tie(y.x, y.y, y.z);
}

template <class Inspector>
bool inspect(Inspector& f, point_3d& x) {
  return f.object(x).fields(f.field("x", x.x), f.field("y", x.y),
                            f.field("z", x.z));
}

struct line {
  point_3d p1;
  point_3d p2;
};

[[maybe_unused]] bool operator==(const line& x, const line& y) {
  return std::tie(x.p1, x.p2) == std::tie(y.p1, y.p2);
}

template <class Inspector>
bool inspect(Inspector& f, line& x) {
  return f.object(x).fields(f.field("p1", x.p1), f.field("p2", x.p2));
}

struct duration {
  std::string unit;
  double count;
};

[[maybe_unused]] bool valid_time_unit(const std::string& unit) {
  return unit == "seconds" || unit == "minutes";
}

template <class Inspector>
bool inspect(Inspector& f, duration& x) {
  return f.object(x).fields(
    f.field("unit", x.unit).fallback("seconds").invariant(valid_time_unit),
    f.field("count", x.count));
}

struct person {
  std::string name;
  std::optional<std::string> phone;
};

template <class Inspector>
bool inspect(Inspector& f, person& x) {
  return f.object(x).fields(f.field("name", x.name), f.field("phone", x.phone));
}

class foobar {
public:
  foobar() = default;
  foobar(const foobar&) = default;
  foobar& operator=(const foobar&) = default;

  foobar(std::string foo, std::string bar)
    : foo_(std::move(foo)), bar_(std::move(bar)) {
    // nop
  }

  const std::string& foo() const {
    return foo_;
  }

  void foo(std::string value) {
    foo_ = std::move(value);
  }

  const std::string& bar() const {
    return bar_;
  }

  void bar(std::string value) {
    bar_ = std::move(value);
  }

private:
  std::string foo_;
  std::string bar_;
};

[[maybe_unused]] bool operator==(const foobar& x, const foobar& y) {
  return std::tie(x.foo(), x.bar()) == std::tie(y.foo(), y.bar());
}

template <class Inspector>
bool inspect(Inspector& f, foobar& x) {
  auto get_foo = [&x]() -> decltype(auto) { return x.foo(); };
  auto set_foo = [&x](std::string value) {
    x.foo(std::move(value));
    return true;
  };
  auto get_bar = [&x]() -> decltype(auto) { return x.bar(); };
  auto set_bar = [&x](std::string value) {
    x.bar(std::move(value));
    return true;
  };
  return f.object(x).fields(f.field("foo", get_foo, set_foo),
                            f.field("bar", get_bar, set_bar));
}

struct dummy_message {
  std::variant<std::string, double> content;
};

[[maybe_unused]] bool operator==(const dummy_message& x,
                                 const dummy_message& y) {
  return x.content == y.content;
}

template <class Inspector>
bool inspect(Inspector& f, dummy_message& x) {
  return f.object(x).fields(f.field("content", x.content));
}

struct fallback_dummy_message {
  std::variant<std::string, double> content;
};

template <class Inspector>
bool inspect(Inspector& f, fallback_dummy_message& x) {
  return f.object(x).fields(f.field("content", x.content).fallback(42.0));
}

struct basics {
  struct tag {};
  tag v1;
  int32_t v2;
  int32_t v3[4];
  dummy_message v4[2];
  std::array<int32_t, 2> v5;
  std::tuple<int32_t, dummy_message> v6;
  std::map<std::string, int32_t> v7;
  std::vector<std::list<std::pair<std::string, std::array<int32_t, 3>>>> v8;
};

template <class Inspector>
bool inspect(Inspector& f, basics& x) {
  return f.object(x).fields(f.field("v1", x.v1), f.field("v2", x.v2),
                            f.field("v3", x.v3), f.field("v4", x.v4),
                            f.field("v5", x.v5), f.field("v6", x.v6),
                            f.field("v7", x.v7), f.field("v8", x.v8));
}

} // namespace
