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

#define CAF_SUITE inspector

#include "core-test.hpp"

#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/meta_object.hpp"
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

struct stringification_inspector_policy {
  template <class T>
  std::string f(T& x) {
    std::string str;
    detail::stringification_inspector fun{str};
    if (!inspect_object(fun, x))
      CAF_FAIL("inspection failed: " << fun.get_error());
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
};

} // namespace

CAF_TEST(stringification_inspector) {
  stringification_inspector_policy p;
  test_impl(p);
}

namespace {

template <class T>
struct is_integral_or_enum {
  static constexpr bool value = std::is_integral<T>::value
                                || std::is_enum<T>::value;
};

struct binary_serialization_policy {
  execution_unit& context;

  template <class T>
  auto to_buf(const T& x) {
    byte_buffer result;
    binary_serializer sink{&context, result};
    if (!inspect_objects(sink, x))
      CAF_FAIL("failed to serialize " << x << ": " << sink.get_error());
    return result;
  }

  template <class T>
  bool operator()(T& x) {
    auto buf = to_buf(x);
    binary_deserializer source{&context, buf};
    auto y = T{};
    if (!inspect_objects(source, y))
      CAF_FAIL("failed to deserialize from buffer: " << source.get_error());
    CAF_CHECK_EQUAL(x, y);
    return detail::safe_equal(x, y);
  }
};

} // namespace

CAF_TEST(binary_serialization_inspectors) {
  actor_system_config cfg;
  actor_system sys{cfg};
  scoped_execution_unit context;
  binary_serialization_policy p{context};
  test_impl(p);
}
