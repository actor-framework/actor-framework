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

#include "caf/config.hpp"

#define CAF_SUITE variant
#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/none.hpp"
#include "caf/variant.hpp"
#include "caf/actor_system.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/actor_system_config.hpp"

using namespace std;
using namespace caf;

struct tostring_visitor : static_visitor<string> {
  template <class T>
  inline string operator()(const T& value) {
    return to_string(value);
  }
};

// 20 integer wrappers for building a variant with 20 distint types
#define i_n(n)                                                                 \
  class i##n {                                                                 \
  public:                                                                      \
    i##n(int y = 0) : x(y) {                                                   \
    }                                                                          \
    i##n(i##n&& other) : x(other.x) {                                          \
      other.x = 0;                                                             \
    }                                                                          \
    i##n& operator=(i##n&& other) {                                            \
      x = other.x;                                                             \
      other.x = 0;                                                             \
      return *this;                                                            \
    }                                                                          \
    i##n(const i##n&) = default;                                               \
    i##n& operator=(const i##n&) = default;                                    \
    int x;                                                                     \
  };                                                                           \
  bool operator==(int x, i##n y) {                                             \
    return x == y.x;                                                           \
  }                                                                            \
  bool operator==(i##n x, int y) {                                             \
    return y == x;                                                             \
  }                                                                            \
  bool operator==(i##n x, i##n y) {                                            \
    return x.x == y.x;                                                         \
  }                                                                            \
  template <class Inspector>                                                   \
  typename Inspector::result_type inspect(Inspector& f, i##n& x) {             \
    return f(meta::type_name(CAF_STR(i##n)), x.x);                             \
  }

#define macro_repeat20(macro)                                                  \
  macro(01) macro(02) macro(03) macro(04) macro(05) macro(06) macro(07)        \
    macro(08) macro(09) macro(10) macro(11) macro(12) macro(13) macro(14)      \
      macro(15) macro(16) macro(17) macro(18) macro(19) macro(20)

macro_repeat20(i_n)

// a variant with 20 element types
using v20 = variant<i01, i02, i03, i04, i05, i06, i07, i08, i09, i10,
                    i11, i12, i13, i14, i15, i16, i17, i18, i19, i20>;

#define announce_n(n) cfg.add_message_type<i##n>(CAF_STR(i##n));

#define v20_test(n)                                                            \
  x3 = i##n{0x##n};                                                            \
  CAF_CHECK_EQUAL(deep_to_string(x3), CAF_STR(i##n) + std::string("(")         \
                                        + std::to_string(0x##n) + ")");        \
  CAF_CHECK_EQUAL(v20{x3}, i##n{0x##n});                                       \
  x4 = x3;                                                                     \
  CAF_CHECK_EQUAL(x4, i##n{0x##n});                                            \
  CAF_CHECK_EQUAL(v20{std::move(x3)}, i##n{0x##n});                            \
  CAF_CHECK_EQUAL(x3, i##n{0});                                                \
  x3 = std::move(x4);                                                          \
  CAF_CHECK_EQUAL(x4, i##n{0});                                                \
  CAF_CHECK_EQUAL(x3, i##n{0x##n});                                            \
  {                                                                            \
    std::vector<char> buf;                                                     \
    binary_serializer bs{sys.dummy_execution_unit(), buf};                     \
    inspect(bs, x3);                                                           \
    CAF_CHECK_EQUAL(x3, i##n{0x##n});                                          \
    v20 tmp;                                                                   \
    binary_deserializer bd{sys.dummy_execution_unit(), buf};                   \
    inspect(bd, tmp);                                                          \
    CAF_CHECK_EQUAL(tmp, i##n{0x##n});                                         \
    CAF_CHECK_EQUAL(tmp, x3);                                                  \
  }

// copy construction, copy assign, move construction, move assign
// and finally serialization round-trip
CAF_TEST(copying_moving_roundtrips) {
  actor_system_config cfg;
  macro_repeat20(announce_n)
  actor_system sys{cfg};
  // default construction
  variant<none_t> x1;
  CAF_CHECK_EQUAL(x1, none);
  variant<int, none_t> x2;
  CAF_CHECK_EQUAL(x2, 0);
  v20 x3;
  CAF_CHECK_EQUAL(x3, i01{0});
  v20 x4;
  macro_repeat20(v20_test);
}

namespace {

struct test_visitor {
  template <class... Ts>
  string operator()(const Ts&... xs) {
    return deep_to_string(std::forward_as_tuple(xs...));
  }
};

} // namespace <anonymous>

CAF_TEST(constructors) {
  variant<int, string> a{42};
  variant<string, atom_value> b{atom("foo")};
  variant<float, int, string> c{string{"bar"}};
  variant<int, string, double> d{123};
  CAF_CHECK_EQUAL(a, 42);
  CAF_CHECK_EQUAL(b, atom("foo"));
  CAF_CHECK_EQUAL(d, 123);
  CAF_CHECK_NOT_EQUAL(d, std::string{"123"});
}

CAF_TEST(n_ary_visit) {
  variant<int, string> a{42};
  variant<string, atom_value> b{atom("foo")};
  variant<float, int, string> c{string{"bar"}};
  variant<int, string, double> d{123};
  test_visitor f;
  CAF_CHECK_EQUAL(visit(f, a, b), "(42, 'foo')");
  CAF_CHECK_EQUAL(visit(f, a, b, c), "(42, 'foo', \"bar\")");
  CAF_CHECK_EQUAL(visit(f, a, b, c, d), "(42, 'foo', \"bar\", 123)");
}

CAF_TEST(get_if) {
  variant<int ,string, atom_value> b = atom("foo");
  CAF_MESSAGE("test get_if directly");
  CAF_CHECK_EQUAL(get_if<int>(&b), nullptr);
  CAF_CHECK_EQUAL(get_if<string>(&b), nullptr);
  CAF_CHECK_NOT_EQUAL(get_if<atom_value>(&b), nullptr);
  CAF_MESSAGE("test get_if via unit test framework");
  CAF_CHECK_NOT_EQUAL(b, 42);
  CAF_CHECK_NOT_EQUAL(b, string{"foo"});
  CAF_CHECK_EQUAL(b, atom("foo"));
}

CAF_TEST(less_than) {
  using variant_type = variant<char, int>;
  auto a = variant_type{'x'};
  auto b = variant_type{'y'};
  CAF_CHECK(a < b);
  CAF_CHECK(!(a > b));
  CAF_CHECK(a <= b);
  CAF_CHECK(!(a >= b));
  b = 42;
  CAF_CHECK(a < b);
  CAF_CHECK(!(a > b));
  CAF_CHECK(a <= b);
  CAF_CHECK(!(a >= b));
  a = 42;
  CAF_CHECK(!(a < b));
  CAF_CHECK(!(a > b));
  CAF_CHECK(a <= b);
  CAF_CHECK(a >= b);
  b = 'x';
}

CAF_TEST(equality) {
  variant<uint16_t, int> x = 42;
  variant<uint16_t, int> y = uint16_t{42};
  CAF_CHECK_NOT_EQUAL(x, y);
}
