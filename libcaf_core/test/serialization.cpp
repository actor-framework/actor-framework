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
#include <tuple>

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

struct common_fixture {
  common_fixture(int32_t i32 = -345, 
                test_enum te = test_enum::b, 
                string str = "Lorem ipsum dolor sit amet.") 
      : i32(i32), te(te), str(str), rs{string(str.rbegin(), str.rend())} {
    msg = make_message(i32, te, str, rs);
  }

  int32_t i32;
  test_enum te;
  string str;
  raw_struct rs;
  message msg;
} fixture = {};

template<typename F, typename T> 
void apply_func(F&& f, T&& t) {
  f(std::forward<T>(t));
}

template<typename F, typename T, typename... Ts> 
void apply_func(F&& f, T&& t, Ts&&... ts) {
  f(std::forward<T>(t));
  apply_func(std::forward<F>(f), std::forward<Ts>(ts)...);
}

struct binary_util {
  static actor_namespace*& serialization_namespace() {
    static actor_namespace* ns = nullptr;
    return ns;
  }

  template <typename T, typename... Ts>
  static string serialize(T&& t, Ts&&... ts) {
    std::stringstream ss;
    binary_serializer bs {
      std::ostreambuf_iterator<char>(ss), 
      serialization_namespace()
    };
    apply_func(
      serializer_lambda{&bs}, 
      std::forward<T>(t), std::forward<Ts>(ts)...
    );
    return ss.str();
  }

  template <typename T, typename... Ts>
  static void deserialize(string const& buff, T* t, Ts*... ts) {
    binary_deserializer bd {
      buff.data(), buff.size(), 
      serialization_namespace()
    };
    apply_func(deserializer_lambda{&bd}, t, ts...);
  }

 private:
  struct serializer_lambda {
    serializer_lambda(binary_serializer* bs) : bs(bs) {}
    binary_serializer* bs;
    template <typename T>
    void operator()(T&& x) const { 
      *bs << x;
    }
  };

  struct deserializer_lambda {
    deserializer_lambda(binary_deserializer* bd) : bd(bd) {}
    binary_deserializer* bd;
    template <typename T>
    void operator()(T* x) const { 
      uniform_typeid<T>()->deserialize(x, bd);
    }
  };
};

struct is_message {
  explicit is_message(message& msg) : msg(msg) {}
  message msg;

  template <typename T, typename... Ts>
  bool equal(T&& t, Ts&&... ts) {
    bool ok = false;
    // work around for gcc bug
    auto tup = tie(t, ts...);
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

CAF_TEST(test_int32_t) {
  auto buff = binary_util::serialize(fixture.i32);
  int32_t i32;
  binary_util::deserialize(buff, &i32);
  CAF_CHECK_EQUAL(fixture.i32, i32);
}

CAF_TEST(test_enum) {
  auto buff = binary_util::serialize(fixture.te);
  test_enum te;
  binary_util::deserialize(buff, &te);
  CAF_CHECK(fixture.te == te);
}

CAF_TEST(test_string) {
  auto buff = binary_util::serialize(fixture.str);
  string str;
  binary_util::deserialize(buff, &str);
  CAF_CHECK_EQUAL(fixture.str, str);
}

CAF_TEST(test_raw_struct) {
  auto buff = binary_util::serialize(fixture.rs);
  raw_struct rs;
  binary_util::deserialize(buff, &rs);
  CAF_CHECK(fixture.rs == rs);
}

CAF_TEST(test_single_message) {
  auto buff = binary_util::serialize(fixture.msg);
  message msg;
  binary_util::deserialize(buff, &msg);
  CAF_CHECK(fixture.msg == msg);
  CAF_CHECK(is_message(msg).equal(fixture.i32, fixture.te, fixture.str, fixture.rs));
}

CAF_TEST(test_multiple_messages) {
  auto m = make_message(fixture.rs, fixture.te);
  auto buff = binary_util::serialize(fixture.te, m, fixture.msg);
  test_enum t;
  message m1;
  message m2;
  binary_util::deserialize(buff, &t, &m1, &m2);
  CAF_CHECK(tie(t, m1, m2) == tie(fixture.te, m, fixture.msg));
  CAF_CHECK(is_message(m1).equal(fixture.rs, fixture.te));
  CAF_CHECK(is_message(m2).equal(fixture.i32, fixture.te, fixture.str, fixture.rs));
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
