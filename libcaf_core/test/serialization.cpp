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

#include "caf/config.hpp"

#define CAF_SUITE serialization
#include "caf/test/unit_test.hpp"

#include <new>
#include <set>
#include <list>
#include <stack>
#include <tuple>
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
#include "caf/await_all_actors_done.hpp"
#include "caf/abstract_uniform_type_info.hpp"

#include "caf/detail/ieee_754.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/get_mac_addresses.hpp"

using namespace std;
using namespace caf;

namespace {

using strmap = map<string, u16string>;

struct raw_struct {
  string str;
};

bool operator==(const raw_struct& lhs, const raw_struct& rhs) {
  return lhs.str == rhs.str;
}

struct raw_struct_type_info : abstract_uniform_type_info<raw_struct> {
  using super = abstract_uniform_type_info<raw_struct>;
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

struct test_array {
  int value[4];
  int value2[2][4];
};

struct test_empty_non_pod {
  test_empty_non_pod() = default;
  test_empty_non_pod(const test_empty_non_pod&) = default;
  virtual void foo() {
    // nop
  }
  virtual ~test_empty_non_pod() {
    // nop
  }
  bool operator==(const test_empty_non_pod&) const {
    return false;
  }
};

struct fixture {
  int32_t i32 = -345;
  test_enum te = test_enum::b;
  string str = "Lorem ipsum dolor sit amet.";
  raw_struct rs;
  test_array ta {
    {0, 1, 2, 3},
    {
      {0, 1, 2, 3},
      {4, 5, 6, 7}
    },
  };
  message msg;

  fixture() {
    announce<test_enum>("test_enum");
    announce(typeid(raw_struct), uniform_type_info_ptr{new raw_struct_type_info});
    announce<test_array>("test_array", &test_array::value, &test_array::value2);
    announce<test_empty_non_pod>("test_empty_non_pod");
    rs.str.assign(string(str.rbegin(), str.rend()));
    msg = make_message(i32, te, str, rs);
  }

  ~fixture() {
    await_all_actors_done();
    shutdown();
  }
};

template <class T>
void apply(binary_serializer& bs, const T& x) {
  bs << x;
}

template <class T>
void apply(binary_deserializer& bd, T* x) {
  uniform_typeid<T>()->deserialize(x, &bd);
}

template <class T>
struct binary_util_fun {
  binary_util_fun(T& ref) : ref_(ref) {
    // nop
  }
  T& ref_;
  void operator()() const {
    // end of recursion
  }
  template <class U, class... Us>
  void operator()(U&& x, Us&&... xs) const {
    apply(ref_, x);
    (*this)(std::forward<Us>(xs)...);
  }
};

class binary_util {
public:
  template <class T, class... Ts>
  static vector<char> serialize(const T& x, const Ts&... xs) {
    vector<char> buf;
    binary_serializer bs{std::back_inserter(buf)};
    binary_util_fun<binary_serializer> f{bs};
    f(x, xs...);
    return buf;
  }

  template <class T, class... Ts>
  static void deserialize(const vector<char>& buf, T* x, Ts*... xs) {
    binary_deserializer bd{buf.data(), buf.size()};
    binary_util_fun<binary_deserializer> f{bd};
    f(x, xs...);
  }
};

struct is_message {
  explicit is_message(message& msgref) : msg(msgref) {
    // nop
  }

  message& msg;

  template <class T, class... Ts>
  bool equal(T&& v, Ts&&... vs) {
    bool ok = false;
    // work around for gcc 4.8.4 bug
    auto tup = tie(v, vs...);
    message_handler impl {
      [&](T const& u, Ts const&... us) {
        ok = tup == tie(u, us...);
      }
    };
    impl(msg);
    return ok;
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(serialization_tests, fixture)

CAF_TEST(test_serialization) {
  using token = std::integral_constant<int, detail::impl_id<strmap>()>;
  CAF_CHECK_EQUAL(detail::is_iterable<strmap>::value, true);
  CAF_CHECK_EQUAL(detail::is_stl_compliant_list<vector<int>>::value, true);
  CAF_CHECK_EQUAL(detail::is_stl_compliant_list<strmap>::value, false);
  CAF_CHECK_EQUAL(detail::is_stl_compliant_map<strmap>::value, true);
  CAF_CHECK_EQUAL(detail::impl_id<strmap>(), 2);
  CAF_CHECK_EQUAL(token::value, 2);

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

CAF_TEST(test_int32_t) {
  auto buf = binary_util::serialize(i32);
  int32_t x;
  binary_util::deserialize(buf, &x);
  CAF_CHECK_EQUAL(i32, x);
}

CAF_TEST(test_enum_serialization) {
  auto buf = binary_util::serialize(te);
  test_enum x;
  binary_util::deserialize(buf, &x);
  CAF_CHECK(te == x);
}

CAF_TEST(test_string) {
  auto buf = binary_util::serialize(str);
  string x;
  binary_util::deserialize(buf, &x);
  CAF_CHECK_EQUAL(str, x);
}

CAF_TEST(test_raw_struct) {
  auto buf = binary_util::serialize(rs);
  raw_struct x;
  binary_util::deserialize(buf, &x);
  CAF_CHECK(rs == x);
}

CAF_TEST(test_array_serialization) {
  auto buf = binary_util::serialize(ta);
  test_array x;
  binary_util::deserialize(buf, &x);
  for (auto i = 0; i < 4; ++i) {
    CAF_CHECK(ta.value[i] == x.value[i]);
  }
  for (auto i = 0; i < 2; ++i) {
    for (auto j = 0; j < 4; ++j) {
      CAF_CHECK(ta.value2[i][j] == x.value2[i][j]);
    }
  }
}

CAF_TEST(test_empty_non_pod_serialization) {
  test_empty_non_pod x;
  auto buf = binary_util::serialize(x);
  binary_util::deserialize(buf, &x);
  CAF_CHECK(true);
}

CAF_TEST(test_single_message) {
  auto buf = binary_util::serialize(msg);
  message x;
  binary_util::deserialize(buf, &x);
  CAF_CHECK(msg == x);
  CAF_CHECK(is_message(x).equal(i32, te, str, rs));
}

CAF_TEST(test_multiple_messages) {
  auto m = make_message(rs, te);
  auto buf = binary_util::serialize(te, m, msg);
  test_enum t;
  message m1;
  message m2;
  binary_util::deserialize(buf, &t, &m1, &m2);
  CAF_CHECK(tie(t, m1, m2) == tie(te, m, msg));
  CAF_CHECK(is_message(m1).equal(rs, te));
  CAF_CHECK(is_message(m2).equal(i32, te, str, rs));
}

CAF_TEST(strings) {
  auto m1 = make_message("hello \"actor world\"!", atom("foo"));
  auto s1 = to_string(m1);
  CAF_CHECK_EQUAL(s1, "@<>+@str+@atom ( \"hello \\\"actor world\\\"!\", 'foo' )");
  CAF_CHECK(from_string<message>(s1) == m1);
  auto m2 = make_message(true);
  auto s2 = to_string(m2);
  CAF_CHECK_EQUAL(s2, "@<>+bool ( true )");
}

CAF_TEST_FIXTURE_SCOPE_END()
