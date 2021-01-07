// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE variant

#include "caf/variant.hpp"

#include "core-test.hpp"

#include <string>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/none.hpp"

using namespace std::string_literals;

using namespace caf;

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
  bool inspect(Inspector& f, i##n& x) {                                        \
    return f.object(x).fields(f.field("x", x.x));                              \
  }

#define macro_repeat20(macro)                                                  \
  macro(01) macro(02) macro(03) macro(04) macro(05) macro(06) macro(07)        \
    macro(08) macro(09) macro(10) macro(11) macro(12) macro(13) macro(14)      \
      macro(15) macro(16) macro(17) macro(18) macro(19) macro(20)

macro_repeat20(i_n)

// a variant with 20 element types
using v20 = variant<i01, i02, i03, i04, i05, i06, i07, i08, i09, i10,
                    i11, i12, i13, i14, i15, i16, i17, i18, i19, i20>;

#define VARIANT_EQ(x, y)                                                       \
  do {                                                                         \
    using type = std::decay_t<decltype(y)>;                                    \
    auto&& tmp = x;                                                            \
    if (CAF_CHECK(holds_alternative<type>(tmp)))                               \
      CAF_CHECK_EQUAL(get<type>(tmp), y);                                      \
  } while (false)

#define v20_test(n)                                                            \
  x3 = i##n{0x##n};                                                            \
  VARIANT_EQ(v20{x3}, i##n{0x##n});                                            \
  x4 = x3;                                                                     \
  VARIANT_EQ(x4, i##n{0x##n});                                                 \
  VARIANT_EQ(v20{std::move(x3)}, i##n{0x##n});                                 \
  VARIANT_EQ(x3, i##n{0});                                                     \
  x3 = std::move(x4);                                                          \
  VARIANT_EQ(x4, i##n{0});                                                     \
  VARIANT_EQ(x3, i##n{0x##n});

// copy construction, copy assign, move construction, move assign
// and finally serialization round-trip
CAF_TEST(copying_moving_roundtrips) {
  actor_system_config cfg;
  actor_system sys{cfg};
  variant<int, none_t> x2;
  VARIANT_EQ(x2, 0);
  v20 x3;
  VARIANT_EQ(x3, i01{0});
  v20 x4;
  macro_repeat20(v20_test);
}

namespace {

struct test_visitor {
  template <class... Ts>
  std::string operator()(const Ts&... xs) {
    return deep_to_string_as_tuple(xs...);
  }
};

} // namespace

CAF_TEST(constructors) {
  variant<int, std::string> a{42};
  variant<float, int, std::string> b{"bar"s};
  variant<int, std::string, double> c{123};
  variant<bool, uint8_t> d{uint8_t{252}};
  VARIANT_EQ(a, 42);
  VARIANT_EQ(b, "bar"s);
  VARIANT_EQ(c, 123);
  VARIANT_EQ(d, uint8_t{252});
}

CAF_TEST(n_ary_visit) {
  variant<int, std::string> a{42};
  variant<float, int, std::string> b{"bar"s};
  variant<int, std::string, double> c{123};
  test_visitor f;
  CAF_CHECK_EQUAL(visit(f, a), R"__([42])__");
  CAF_CHECK_EQUAL(visit(f, a, b), R"__([42, "bar"])__");
  CAF_CHECK_EQUAL(visit(f, a, b, c), R"__([42, "bar", 123])__");
}

CAF_TEST(get_if) {
  variant<int, std::string> b = "foo"s;
  CAF_MESSAGE("test get_if directly");
  CAF_CHECK_EQUAL(get_if<int>(&b), nullptr);
  CAF_CHECK_NOT_EQUAL(get_if<std::string>(&b), nullptr);
  CAF_MESSAGE("test get_if via unit test framework");
  VARIANT_EQ(b, "foo"s);
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
