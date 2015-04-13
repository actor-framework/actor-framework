/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE serialization
#include "caf/test/unit_test.hpp"

#include "caf/config.hpp"

#include <new>
#include <set>
#include <list>
#include <stack>
#include <locale>
#include <memory>
#include <string>
#include <limits>
#include <vector>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <iterator>
#include <typeinfo>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <type_traits>

#include "caf/message.hpp"
#include "caf/announce.hpp"
#include "caf/shutdown.hpp"
#include "caf/to_string.hpp"
#include "caf/serializer.hpp"
#include "caf/from_string.hpp"
#include "caf/ref_counted.hpp"
#include "caf/deserializer.hpp"
#include "caf/actor_namespace.hpp"
#include "caf/primitive_variant.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/detail/get_mac_addresses.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/abstract_uniform_type_info.hpp"

#include "caf/detail/ieee_754.hpp"
#include "caf/detail/safe_equal.hpp"

using namespace std;
using namespace caf;

namespace {

struct struct_a {
  int x;
  int y;
};

struct struct_b {
  struct_a a;
  int z;
  list<int> ints;
};

using strmap = map<string, u16string>;

struct struct_c {
  strmap strings;
  set<int> ints;
};

struct raw_struct {
  string str;
};

bool operator==(const raw_struct& lhs, const raw_struct& rhs) {
  return lhs.str == rhs.str;
}

struct raw_struct_type_info : detail::abstract_uniform_type_info<raw_struct> {
  using super = detail::abstract_uniform_type_info<raw_struct>;
  raw_struct_type_info() : super("raw_struct") {
    // nop
  }
  void serialize(const void* ptr, serializer* sink) const override {
    auto rs = reinterpret_cast<const raw_struct*>(ptr);
    sink->write_value(static_cast<uint32_t>(rs->str.size()));
    sink->write_raw(rs->str.size(), rs->str.data());
  }
  void deserialize(void* ptr, deserializer* source) const override {
    auto rs = reinterpret_cast<raw_struct*>(ptr);
    rs->str.clear();
    auto size = source->read<uint32_t>();
    rs->str.resize(size);
    source->read_raw(size, &(rs->str[0]));
  }
};



enum class test_enum {
  a,
  b,
  c
};

} // namespace <anonymous>

CAF_TEST(test_serialization) {
  announce<test_enum>("test_enum");
  using token = std::integral_constant<int, detail::impl_id<strmap>()>;
  CAF_CHECK_EQUAL(detail::is_iterable<strmap>::value, true);
  CAF_CHECK_EQUAL(detail::is_stl_compliant_list<vector<int>>::value, true);
  CAF_CHECK_EQUAL(detail::is_stl_compliant_list<strmap>::value, false);
  CAF_CHECK_EQUAL(detail::is_stl_compliant_map<strmap>::value, true);
  CAF_CHECK_EQUAL(detail::impl_id<strmap>(), 2);
  CAF_CHECK_EQUAL(token::value, 2);

  announce(typeid(raw_struct), uniform_type_info_ptr{new raw_struct_type_info});

  auto nid = detail::singletons::get_node_id();
  auto nid_str = to_string(nid);
  CAF_MESSAGE("nid_str = " << nid_str);
  auto nid2 = from_string<node_id>(nid_str);
  CAF_CHECK(nid2);
  if (nid2) {
    CAF_CHECK_EQUAL(to_string(nid), to_string(*nid2));
  }
}

CAF_TEST(test_ieee_754) {
  // check conversion of float
  float f1 = 3.1415925f;         // float value
  auto p1 = caf::detail::pack754(f1); // packet value
  CAF_CHECK_EQUAL(p1, 0x40490FDA);
  auto u1 = caf::detail::unpack754(p1); // unpacked value
  CAF_CHECK_EQUAL(f1, u1);
  // check conversion of double
  double f2 = 3.14159265358979311600;  // double value
  auto p2 = caf::detail::pack754(f2); // packet value
  CAF_CHECK_EQUAL(p2, 0x400921FB54442D18);
  auto u2 = caf::detail::unpack754(p2); // unpacked value
  CAF_CHECK_EQUAL(f2, u2);
}

CAF_TEST(test_string_serialization) {
  auto input = make_message("hello \"actor world\"!", atom("foo"));
  auto s = to_string(input);
  CAF_CHECK_EQUAL(s, R"#(@<>+@str+@atom ( "hello \"actor world\"!", 'foo' ))#");
  auto m = from_string<message>(s);
  if (!m) {
    CAF_TEST_ERROR("from_string failed");
    return;
  }
  CAF_CHECK(*m == input);
  CAF_CHECK_EQUAL(to_string(*m), to_string(input));
  shutdown();
}
