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

#include "test.hpp"

#include "caf/message.hpp"
#include "caf/announce.hpp"
#include "caf/message.hpp"
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
  bool equals(const void* lhs, const void* rhs) const override {
    return deref(lhs) == deref(rhs);
  }

};

void test_ieee_754() {
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

enum class test_enum {
  a,
  b,
  c
};

} // namespace <anonymous>

int main() {
  CAF_TEST(test_serialization);

  announce<test_enum>();

  test_ieee_754();

  using token = std::integral_constant<int, detail::impl_id<strmap>()>;
  CAF_CHECK_EQUAL(detail::is_iterable<strmap>::value, true);
  CAF_CHECK_EQUAL(detail::is_stl_compliant_list<vector<int>>::value, true);
  CAF_CHECK_EQUAL(detail::is_stl_compliant_list<strmap>::value, false);
  CAF_CHECK_EQUAL(detail::is_stl_compliant_map<strmap>::value, true);
  CAF_CHECK_EQUAL(detail::impl_id<strmap>(), 2);
  CAF_CHECK_EQUAL(token::value, 2);

  announce(typeid(raw_struct),
       uniform_type_info_ptr{new raw_struct_type_info});

  auto nid = detail::singletons::get_node_id();
  auto nid_str = to_string(nid);
  CAF_PRINT("nid_str = " << nid_str);
  auto nid2 = from_string<node_id>(nid_str);
  CAF_CHECK(nid2);
  if (nid2) {
    CAF_CHECK_EQUAL(to_string(nid), to_string(*nid2));
  }

  /*
    auto oarr = new detail::object_array;
    oarr->push_back(object::from(static_cast<uint32_t>(42)));
    oarr->push_back(object::from("foo"  ));

    message atuple1{static_cast<message::raw_ptr>(oarr)};
    try {
      bool ok = false;
      message_handler check {
        [&](uint32_t val0, string val1) {
          ok = (val0 == 42 && val1 == "foo");
        }

      };
      check(atuple1);
      CAF_CHECK(ok);
    }
    catch (std::exception& e) { CAF_FAILURE(to_verbose_string(e)); }

    detail::meta_cow_tuple<int,int> mct;
    try {
      auto tup0 = make_cow_tuple(1, 2);
      io::buffer wr_buf;
      binary_serializer bs(&wr_buf, &addressing);
      mct.serialize(&tup0, &bs);
      binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
      auto ptr = mct.new_instance();
      auto ptr_guard = make_scope_guard([&] {
        mct.delete_instance(ptr);
      });
      mct.deserialize(ptr, &bd);
      auto& tref = *reinterpret_cast<cow_tuple<int, int>*>(ptr);
      CAF_CHECK_EQUAL(get<0>(tup0), get<0>(tref));
      CAF_CHECK_EQUAL(get<1>(tup0), get<1>(tref));
    }
    catch (std::exception& e) { CAF_FAILURE(to_verbose_string(e)); }

    try {
      // test raw_type in both binary and string serialization
      raw_struct rs;
      rs.str = "Lorem ipsum dolor sit amet.";
      io::buffer wr_buf;
      binary_serializer bs(&wr_buf, &addressing);
      bs << rs;
      binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
      raw_struct rs2;
      uniform_typeid<raw_struct>()->deserialize(&rs2, &bd);
      CAF_CHECK_EQUAL(rs2.str, rs.str);
      auto rsstr = caf::to_string(object::from(rs));
      rs2.str = "foobar";
      CAF_PRINT("rs as string: " << rsstr);
      rs2 = from_string<raw_struct>(rsstr);
      CAF_CHECK_EQUAL(rs2.str, rs.str);
    }
    catch (std::exception& e) { CAF_FAILURE(to_verbose_string(e)); }

    try {
      scoped_actor self;
      auto ttup = make_message(1, 2, actor{self.get()});
      io::buffer wr_buf;
      binary_serializer bs(&wr_buf, &addressing);
      bs << ttup;
      binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
      message ttup2;
      uniform_typeid<message>()->deserialize(&ttup2, &bd);
      CAF_CHECK(ttup  == ttup2);
    }
    catch (std::exception& e) { CAF_FAILURE(to_verbose_string(e)); }

    try {
      scoped_actor self;
      auto ttup = make_message(1, 2, actor{self.get()});
      io::buffer wr_buf;
      binary_serializer bs(&wr_buf, &addressing);
      bs << ttup;
      bs << ttup;
      binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
      message ttup2;
      message ttup3;
      uniform_typeid<message>()->deserialize(&ttup2, &bd);
      uniform_typeid<message>()->deserialize(&ttup3, &bd);
      CAF_CHECK(ttup  == ttup2);
      CAF_CHECK(ttup  == ttup3);
      CAF_CHECK(ttup2 == ttup3);
    }
    catch (std::exception& e) { CAF_FAILURE(to_verbose_string(e)); }

    try {
      // serialize b1 to buf
      io::buffer wr_buf;
      binary_serializer bs(&wr_buf, &addressing);
      bs << atuple1;
      // deserialize b2 from buf
      binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
      message atuple2;
      uniform_typeid<message>()->deserialize(&atuple2, &bd);
      bool ok = false;
      message_handler check {
        [&](uint32_t val0, string val1) {
          ok = (val0 == 42 && val1 == "foo");
        }

      };
      check(atuple2);
      CAF_CHECK(ok);
    }
    catch (std::exception& e) { CAF_FAILURE(to_verbose_string(e)); }

    CAF_CHECK((is_iterable<int>::value) == false);
    // string is primitive and thus not identified by is_iterable
    CAF_CHECK((is_iterable<string>::value) == false);
    CAF_CHECK((is_iterable<list<int>>::value) == true);
    CAF_CHECK((is_iterable<map<int, int>>::value) == true);
    try {  // test meta_object implementation for primitive types
      auto meta_int = uniform_typeid<uint32_t>();
      CAF_CHECK(meta_int != nullptr);
      if (meta_int) {
        auto o = meta_int->create();
        get_ref<uint32_t>(o) = 42;
        auto str = to_string(object::from(get<uint32_t>(o)));
        CAF_CHECK_EQUAL( "@u32 ( 42 )", str);
      }
    }
    catch (std::exception& e) { CAF_FAILURE(to_verbose_string(e)); }

    // test serialization of enums
    try {
      auto enum_tuple = make_message(test_enum::b);
      // serialize b1 to buf
      io::buffer wr_buf;
      binary_serializer bs(&wr_buf, &addressing);
      bs << enum_tuple;
      // deserialize b2 from buf
      binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
      message enum_tuple2;
      uniform_typeid<message>()->deserialize(&enum_tuple2, &bd);
      bool ok = false;
      message_handler check {
        [&](test_enum val) {
          ok = (val == test_enum::b);
        }

      };
      check(enum_tuple2);
      CAF_CHECK(ok);
    }
    catch (std::exception& e) { CAF_FAILURE(to_verbose_string(e)); }
  */
  return CAF_TEST_RESULT();
}
