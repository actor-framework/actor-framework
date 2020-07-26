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

#define CAF_SUITE load_inspector

#include "caf/load_inspector.hpp"

#include "caf/test/dsl.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace caf {

template <>
struct inspector_access<std::string> {
  template <class Inspector>
  static bool apply(Inspector& f, std::string& x) {
    return f.value(x);
  }
};

template <>
struct inspector_access<int32_t> {
  template <class Inspector>
  static bool apply(Inspector& f, int32_t& x) {
    return f.value(x);
  }
};

template <>
struct inspector_access<double> {
  template <class Inspector>
  static bool apply(Inspector& f, double& x) {
    return f.value(x);
  }
};

} // namespace caf

using namespace caf;

namespace {

using string_list = std::vector<std::string>;

struct point_3d {
  static inline string_view tname = "point_3d";
  int32_t x;
  int32_t y;
  int32_t z;
};

template <class Inspector>
bool inspect(Inspector& f, point_3d& x) {
  return f.object(x).fields(f.field("x", x.x), f.field("y", x.y),
                            f.field("z", x.z));
}

struct line {
  static inline string_view tname = "line";
  point_3d p1;
  point_3d p2;
};

template <class Inspector>
bool inspect(Inspector& f, line& x) {
  return f.object(x).fields(f.field("p1", x.p1), f.field("p2", x.p2));
}

struct duration {
  static inline string_view tname = "duration";
  std::string unit;
  double count;
};

bool valid_time_unit(const std::string& unit) {
  return unit == "seconds" || unit == "minutes";
}

template <class Inspector>
bool inspect(Inspector& f, duration& x) {
  return f.object(x).fields(
    f.field("unit", x.unit).fallback("seconds").invariant(valid_time_unit),
    f.field("count", x.count));
}

struct person {
  static inline string_view tname = "person";
  std::string name;
  optional<std::string> phone;
};

template <class Inspector>
bool inspect(Inspector& f, person& x) {
  return f.object(x).fields(f.field("name", x.name), f.field("phone", x.phone));
}

class foobar {
public:
  static inline string_view tname = "foobar";

  const std::string& foo() {
    return foo_;
  }

  void foo(std::string value) {
    foo_ = std::move(value);
  }

  const std::string& bar() {
    return bar_;
  }

  void bar(std::string value) {
    bar_ = std::move(value);
  }

private:
  std::string foo_;
  std::string bar_;
};

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

struct testee : load_inspector {
  std::string log;

  error err;

  void set_error(error x) {
    err = std::move(x);
  }

  size_t indent = 0;

  void new_line() {
    log += '\n';
    log.insert(log.end(), indent, ' ');
  }

  template <class T>
  auto object(T&) {
    return object_t<testee>{T::tname, this};
  }

  bool begin_object(string_view object_name) {
    new_line();
    indent += 2;
    log += "begin object ";
    log.insert(log.end(), object_name.begin(), object_name.end());
    return ok;
  }

  bool end_object() {
    indent -= 2;
    new_line();
    log += "end object";
    return ok;
  }

  bool begin_field(string_view name) {
    new_line();
    indent += 2;
    log += "begin field ";
    log.insert(log.end(), name.begin(), name.end());
    return ok;
  }

  bool begin_field(string_view name, bool& is_present) {
    new_line();
    indent += 2;
    log += "begin optional field ";
    log.insert(log.end(), name.begin(), name.end());
    is_present = false;
    return ok;
  }

  bool end_field() {
    indent -= 2;
    new_line();
    log += "end field";
    return ok;
  }

  template <class T>
  bool value(T& x) {
    new_line();
    log += type_name_v<T>;
    log += " value";
    x = T{};
    return ok;
  }
};

struct fixture {
  testee f;

};

} // namespace

CAF_TEST_FIXTURE_SCOPE(load_inspector_tests, fixture)

CAF_TEST(load inspectors can visit simple POD types) {
  point_3d p{1, 1, 1};
  CAF_CHECK_EQUAL(inspect(f, p), true);
  CAF_CHECK_EQUAL(p.x, 0);
  CAF_CHECK_EQUAL(p.y, 0);
  CAF_CHECK_EQUAL(p.z, 0);
  CAF_CHECK_EQUAL(f.log, R"_(
begin object point_3d
  begin field x
    int32_t value
  end field
  begin field y
    int32_t value
  end field
  begin field z
    int32_t value
  end field
end object)_");
}

CAF_TEST(load inspectors recurse into members) {
  line l{point_3d{1, 1, 1}, point_3d{1, 1, 1}};
  CAF_CHECK_EQUAL(inspect(f, l), true);
  CAF_CHECK_EQUAL(l.p1.x, 0);
  CAF_CHECK_EQUAL(l.p1.y, 0);
  CAF_CHECK_EQUAL(l.p1.z, 0);
  CAF_CHECK_EQUAL(l.p2.x, 0);
  CAF_CHECK_EQUAL(l.p2.y, 0);
  CAF_CHECK_EQUAL(l.p2.z, 0);
  CAF_CHECK_EQUAL(f.log, R"_(
begin object line
  begin field p1
    begin object point_3d
      begin field x
        int32_t value
      end field
      begin field y
        int32_t value
      end field
      begin field z
        int32_t value
      end field
    end object
  end field
  begin field p2
    begin object point_3d
      begin field x
        int32_t value
      end field
      begin field y
        int32_t value
      end field
      begin field z
        int32_t value
      end field
    end object
  end field
end object)_");
}

CAF_TEST(load inspectors support fields with fallbacks and invariants) {
  duration d{"minutes", 42};
  CAF_CHECK_EQUAL(inspect(f, d), true);
  CAF_CHECK_EQUAL(d.unit, "seconds");
  CAF_CHECK_EQUAL(d.count, 0.0);
  CAF_CHECK_EQUAL(f.log, R"_(
begin object duration
  begin optional field unit
  end field
  begin field count
    double value
  end field
end object)_");
}

CAF_TEST(load inspectors support fields with optional values) {
  person p{"Bruce Almighty", std::string{"776-2323"}};
  CAF_CHECK_EQUAL(inspect(f, p), true);
  CAF_CHECK_EQUAL(p.name, "");
  CAF_CHECK_EQUAL(p.phone, none);
  CAF_CHECK_EQUAL(f.log, R"_(
begin object person
  begin field name
    std::string value
  end field
  begin optional field phone
  end field
end object)_");
}

CAF_TEST(load inspectors support fields with getters and setters) {
  foobar fb;
  fb.foo("hello");
  fb.bar("world");
  CAF_CHECK_EQUAL(inspect(f, fb), true);
  CAF_CHECK_EQUAL(fb.foo(), "");
  CAF_CHECK_EQUAL(fb.bar(), "");
  CAF_CHECK_EQUAL(f.log, R"_(
begin object foobar
  begin field foo
    std::string value
  end field
  begin field bar
    std::string value
  end field
end object)_");
}

CAF_TEST_FIXTURE_SCOPE_END()
