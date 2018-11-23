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

#define CAF_SUITE sum_type
#include "caf/test/unit_test.hpp"

#include <new>
#include <map>
#include <string>

#include "caf/default_sum_type_access.hpp"
#include "caf/detail/overload.hpp"
#include "caf/raise_error.hpp"
#include "caf/static_visitor.hpp"
#include "caf/sum_type.hpp"
#include "caf/sum_type_access.hpp"

namespace {

class union_type {
public:
  friend struct caf::default_sum_type_access<union_type>;

  using T0 = int;
  using T1 = std::string;
  using T2 = std::map<int, int>;

  using types = caf::detail::type_list<T0, T1, T2>;

  union_type() : index_(0), v0(0) {
    // nop
  }

  ~union_type() {
    destroy();
  }

  union_type& operator=(T0 value) {
    destroy();
    index_ = 0;
    v0 = value;
    return *this;
  }

  union_type& operator=(T1 value) {
    destroy();
    index_ = 1;
    new (&v1) T1(std::move(value));
    return *this;
  }

  union_type& operator=(T2 value) {
    destroy();
    index_ = 2;
    new (&v2) T2(std::move(value));
    return *this;
  }

private:
  inline union_type& get_data() {
    return *this;
  }

  inline const union_type& get_data() const {
    return *this;
  }

  inline T0& get(std::integral_constant<int, 0>) {
    CAF_REQUIRE_EQUAL(index_, 0);
    return v0;
  }

  inline const T0& get(std::integral_constant<int, 0>) const {
    CAF_REQUIRE_EQUAL(index_, 0);
    return v0;
  }

  inline T1& get(std::integral_constant<int, 1>) {
    CAF_REQUIRE_EQUAL(index_, 1);
    return v1;
  }

  inline const T1& get(std::integral_constant<int, 1>) const {
    CAF_REQUIRE_EQUAL(index_, 1);
    return v1;
  }

  inline T2& get(std::integral_constant<int, 2>) {
    CAF_REQUIRE_EQUAL(index_, 2);
    return v2;
  }

  inline const T2& get(std::integral_constant<int, 2>) const {
    CAF_REQUIRE_EQUAL(index_, 2);
    return v2;
  }

  template <int Index>
  inline bool is(std::integral_constant<int, Index>) const {
    return index_ == Index;
  }

  template <class Result, class Visitor, class... Ts>
  inline Result apply(Visitor&& f, Ts&&... xs) const {
    switch (index_) {
      case 0: return f(std::forward<Ts>(xs)..., v0);
      case 1: return f(std::forward<Ts>(xs)..., v1);
      case 2: return f(std::forward<Ts>(xs)..., v2);
    }
    CAF_RAISE_ERROR("invalid index in union_type");
  }

  void destroy() {
    if (index_ == 1)
      v1.~T1();
    else if (index_ == 2)
      v2.~T2();
  }

  int index_;
  union {
    T0 v0;
    T1 v1;
    T2 v2;
  };
};

} // namespace <anonymous>

namespace caf {

template <>
struct sum_type_access<union_type> : default_sum_type_access<union_type> {};

} // namespace caf

using namespace caf;

using std::string;
using map_type = std::map<int, int>;

namespace {

struct stringify_t {
  string operator()(int x) const {
    return std::to_string(x);
  }

  string operator()(std::string x) const {
    return x;
  }

  string operator()(const map_type& x) const {
    return deep_to_string(x);
  }

  template <class T0, class T1>
  string operator()(const T0& x0, const T1& x1) const {
    return (*this)(x0) + ", " + (*this)(x1);
  }

  template <class T0, class T1, class T2>
  string operator()(const T0& x0, const T1& x1, const T2& x2) const {
    return (*this)(x0, x1) + ", " + (*this)(x2);
  }
};

constexpr stringify_t stringify = stringify_t{};

} // namespace <anonymous>

CAF_TEST(holds_alternative) {
  union_type x;
  CAF_CHECK_EQUAL(holds_alternative<int>(x), true);
  CAF_CHECK_EQUAL(holds_alternative<string>(x), false);
  CAF_CHECK_EQUAL(holds_alternative<map_type>(x), false);
  x = string{"hello world"};
  CAF_CHECK_EQUAL(holds_alternative<int>(x), false);
  CAF_CHECK_EQUAL(holds_alternative<string>(x), true);
  CAF_CHECK_EQUAL(holds_alternative<map_type>(x), false);
  x = map_type{{1, 1}, {2, 2}};
  CAF_CHECK_EQUAL(holds_alternative<int>(x), false);
  CAF_CHECK_EQUAL(holds_alternative<string>(x), false);
  CAF_CHECK_EQUAL(holds_alternative<map_type>(x), true);
}

CAF_TEST(get) {
  union_type x;
  CAF_CHECK_EQUAL(get<int>(x), 0);
  x = 42;
  CAF_CHECK_EQUAL(get<int>(x), 42);
  x = string{"hello world"};
  CAF_CHECK_EQUAL(get<string>(x), "hello world");
  x = map_type{{1, 1}, {2, 2}};
  CAF_CHECK_EQUAL(get<map_type>(x), map_type({{1, 1}, {2, 2}}));
}

CAF_TEST(get_if) {
  union_type x;
  CAF_CHECK_EQUAL(get_if<int>(&x), &get<int>(x));
  CAF_CHECK_EQUAL(get_if<string>(&x), nullptr);
  CAF_CHECK_EQUAL(get_if<map_type>(&x), nullptr);
  x = string{"hello world"};
  CAF_CHECK_EQUAL(get_if<int>(&x), nullptr);
  CAF_CHECK_EQUAL(get_if<string>(&x), &get<string>(x));
  CAF_CHECK_EQUAL(get_if<map_type>(&x), nullptr);
  x = map_type{{1, 1}, {2, 2}};
  CAF_CHECK_EQUAL(get_if<int>(&x), nullptr);
  CAF_CHECK_EQUAL(get_if<string>(&x), nullptr);
  CAF_CHECK_EQUAL(get_if<map_type>(&x), &get<map_type>(x));
}

CAF_TEST(unary visit) {
  union_type x;
  CAF_CHECK_EQUAL(visit(stringify, x), "0");
  x = string{"hello world"};
  CAF_CHECK_EQUAL(visit(stringify, x), "hello world");
  x = map_type{{1, 1}, {2, 2}};
  CAF_CHECK_EQUAL(visit(stringify, x), "[(1, 1), (2, 2)]");
}

CAF_TEST(binary visit) {
  union_type x;
  union_type y;
  CAF_CHECK_EQUAL(visit(stringify, x, y), "0, 0");
  x = 42;
  y = string{"hello world"};
  CAF_CHECK_EQUAL(visit(stringify, x, y), "42, hello world");
}

CAF_TEST(ternary visit) {
  union_type x;
  union_type y;
  union_type z;
  //CAF_CHECK_EQUAL(visit(stringify, x, y, z), "0, 0, 0");
  x = 42;
  y = string{"foo"};
  z = map_type{{1, 1}, {2, 2}};
  CAF_CHECK_EQUAL(visit(stringify, x, y, z), "42, foo, [(1, 1), (2, 2)]");
}
