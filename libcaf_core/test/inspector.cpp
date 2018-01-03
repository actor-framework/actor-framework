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

#include <set>
#include <map>
#include <list>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>

#include "caf/config.hpp"

#define CAF_SUITE inspector
#include "caf/test/unit_test.hpp"

#include "caf/actor_system.hpp"
#include "caf/type_erased_value.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/make_type_erased_value.hpp"

#include "caf/detail/stringification_inspector.hpp"

using namespace caf;

namespace {

template <class T>
class inspector_adapter_base {
public:
  inspector_adapter_base(T& impl) : impl_(impl) {
    // nop
  }

protected:
  T& impl_;
};

template <class RoundtripPolicy>
struct check_impl {
  RoundtripPolicy& p_;

  template <class T>
  bool operator()(T x) {
    return p_(x);
  }
};

template <class T>
using nl = std::numeric_limits<T>;

// an empty type
struct dummy_tag_type {};

constexpr bool operator==(dummy_tag_type, dummy_tag_type) {
  return true;
}

// a POD type
struct dummy_struct {
  int a;
  std::string b;
};

bool operator==(const dummy_struct& x, const dummy_struct& y) {
  return x.a == y.a && x.b == y.b;
}

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, dummy_struct& x) {
  return f(meta::type_name("dummy_struct"), x.a, x.b);
}

// two different styles of enums
enum dummy_enum { de_foo, de_bar };

enum dummy_enum_class : short { foo, bar };

std::string to_string(dummy_enum_class x) {
  return x == dummy_enum_class::foo ? "foo" : "bar";
}

template <class Policy>
void test_impl(Policy& p) {
  check_impl<Policy> check{p};
  // check primitive types
  CAF_CHECK(check(true));
  CAF_CHECK(check(false));
  CAF_CHECK(check(nl<int8_t>::lowest()));
  CAF_CHECK(check(nl<int8_t>::max()));
  CAF_CHECK(check(nl<uint8_t>::lowest()));
  CAF_CHECK(check(nl<uint8_t>::max()));
  CAF_CHECK(check(nl<int16_t>::lowest()));
  CAF_CHECK(check(nl<int16_t>::max()));
  CAF_CHECK(check(nl<uint16_t>::lowest()));
  CAF_CHECK(check(nl<uint16_t>::max()));
  CAF_CHECK(check(nl<int32_t>::lowest()));
  CAF_CHECK(check(nl<int32_t>::max()));
  CAF_CHECK(check(nl<uint32_t>::lowest()));
  CAF_CHECK(check(nl<uint32_t>::max()));
  CAF_CHECK(check(nl<int64_t>::lowest()));
  CAF_CHECK(check(nl<int64_t>::max()));
  CAF_CHECK(check(nl<uint64_t>::lowest()));
  CAF_CHECK(check(nl<uint64_t>::max()));
  CAF_CHECK(check(nl<float>::lowest()));
  CAF_CHECK(check(nl<float>::max()));
  CAF_CHECK(check(nl<double>::lowest()));
  CAF_CHECK(check(nl<double>::max()));
  CAF_CHECK(check(nl<long double>::lowest()));
  CAF_CHECK(check(nl<long double>::max()));
  // atoms
  CAF_CHECK(check(atom("")));
  CAF_CHECK(check(atom("0123456789")));
  // various containers
  CAF_CHECK(check(std::array<int, 3>{{1, 2, 3}}));
  CAF_CHECK(check(std::vector<char>{}));
  CAF_CHECK(check(std::vector<char>{1, 2, 3}));
  CAF_CHECK(check(std::vector<int>{}));
  CAF_CHECK(check(std::vector<int>{1, 2, 3}));
  CAF_CHECK(check(std::list<int>{}));
  CAF_CHECK(check(std::list<int>{1, 2, 3}));
  CAF_CHECK(check(std::set<int>{}));
  CAF_CHECK(check(std::set<int>{1, 2, 3}));
  CAF_CHECK(check(std::unordered_set<int>{}));
  CAF_CHECK(check(std::unordered_set<int>{1, 2, 3}));
  CAF_CHECK(check(std::map<int, int>{}));
  CAF_CHECK(check(std::map<int, int>{{1, 1}, {2, 2}, {3, 3}}));
  CAF_CHECK(check(std::unordered_map<int, int>{}));
  CAF_CHECK(check(std::unordered_map<int, int>{{1, 1}, {2, 2}, {3, 3}}));
  // user-defined types
  CAF_CHECK(check(dummy_struct{10, "hello"}));
  // optionals
  CAF_CHECK(check(optional<int>{}));
  CAF_CHECK(check(optional<int>{42}));
  // strings
  CAF_CHECK(check(std::string{}));
  CAF_CHECK(check(std::string{""}));
  CAF_CHECK(check(std::string{"test"}));
  CAF_CHECK(check(std::u16string{}));
  CAF_CHECK(check(std::u16string{u""}));
  CAF_CHECK(check(std::u16string{u"test"}));
  CAF_CHECK(check(std::u32string{}));
  CAF_CHECK(check(std::u32string{U""}));
  CAF_CHECK(check(std::u32string{U"test"}));
  // enums
  CAF_CHECK(check(de_foo));
  CAF_CHECK(check(de_bar));
  CAF_CHECK(check(dummy_enum_class::foo));
  CAF_CHECK(check(dummy_enum_class::bar));
  // empty type
  CAF_CHECK(check(dummy_tag_type{}));
  // pair and tuple
  CAF_CHECK(check(std::make_pair(std::string("hello"), 42)));
  CAF_CHECK(check(std::make_pair(std::make_pair(1, 2), 3)));
  CAF_CHECK(check(std::make_pair(std::make_tuple(1, 2), 3)));
  CAF_CHECK(check(std::make_tuple(1, 2, 3, 4)));
  CAF_CHECK(check(std::make_tuple(std::make_tuple(1, 2, 3), 4)));
  CAF_CHECK(check(std::make_tuple(std::make_pair(1, 2), 3, 4)));
  // variant<>
  CAF_CHECK(check(variant<none_t>{}));
  CAF_CHECK(check(variant<none_t, int, std::string>{}));
  CAF_CHECK(check(variant<none_t, int, std::string>{42}));
  CAF_CHECK(check(variant<none_t, int, std::string>{std::string{"foo"}}));
}

} // namespace <anonymous>

namespace {
struct stringification_inspector_policy {
  template <class T>
  std::string f(T& x) {
    std::string str;
    detail::stringification_inspector fun{str};
    fun(x);
    return str;
  }

  // only check for compilation for complex types
  template <class T>
  typename std::enable_if<!std::is_integral<T>::value, bool>::type
  operator()(T& x) {
    CAF_MESSAGE("f(x) = " << f(x));
    return true;
  }

  // check result for integral types
  template <class T>
  typename std::enable_if<std::is_integral<T>::value, bool>::type
  operator()(T& x) {
    CAF_CHECK_EQUAL(f(x), std::to_string(x));
    return true;
  }

  // check result for bool
  bool operator()(bool& x) {
    CAF_CHECK_EQUAL(f(x), std::string{x ? "true" : "false"});
    return true;
  }

  // check result for atoms
  bool operator()(atom_value& x) {
    CAF_CHECK_EQUAL(f(x), "'" + to_string(x) + "'");
    return true;
  }
};
} // namespace <anonymous>

CAF_TEST(stringification_inspector) {
  stringification_inspector_policy p;
  test_impl(p);
}

namespace {
struct binary_serialization_policy {
  execution_unit& context;

  template <class T>
  bool operator()(T& x) {
    std::vector<char> buf;
    binary_serializer f{&context, buf};
    f(x);
    binary_deserializer g{&context, buf};
    T y;
    g(y);
    CAF_CHECK_EQUAL(x, y);
    return detail::safe_equal(x, y);
  }
};
} // namespace <anonymous>

CAF_TEST(binary_serialization_inspectors) {
  actor_system_config cfg;
  cfg.add_message_type<dummy_struct>("dummy_struct");
  actor_system sys{cfg};
  scoped_execution_unit context;
  binary_serialization_policy p{context};
  test_impl(p);

}
