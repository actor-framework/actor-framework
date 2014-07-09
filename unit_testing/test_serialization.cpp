#include "cppa/config.hpp"

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

#include "cppa/message.hpp"
#include "cppa/announce.hpp"
#include "cppa/message.hpp"
#include "cppa/to_string.hpp"
#include "cppa/serializer.hpp"
#include "cppa/from_string.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/actor_namespace.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"
#include "cppa/detail/get_mac_addresses.hpp"

#include "cppa/detail/int_list.hpp"
#include "cppa/detail/type_traits.hpp"
#include "cppa/detail/abstract_uniform_type_info.hpp"

#include "cppa/detail/ieee_754.hpp"
#include "cppa/detail/safe_equal.hpp"

using namespace std;
using namespace cppa;

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
    float f1 = 3.1415925f;               // float value
    auto p1 = cppa::detail::pack754(f1); // packet value
    CPPA_CHECK_EQUAL(p1, 0x40490FDA);
    auto u1 = cppa::detail::unpack754(p1); // unpacked value
    CPPA_CHECK_EQUAL(f1, u1);
    // check conversion of double
    double f2 = 3.14159265358979311600;  // double value
    auto p2 = cppa::detail::pack754(f2); // packet value
    CPPA_CHECK_EQUAL(p2, 0x400921FB54442D18);
    auto u2 = cppa::detail::unpack754(p2); // unpacked value
    CPPA_CHECK_EQUAL(f2, u2);
}

enum class test_enum {
    a,
    b,
    c

};

} // namespace <anonymous>

int main() {
    CPPA_TEST(test_serialization);

    announce<test_enum>();

    test_ieee_754();

    using token = std::integral_constant<int, detail::impl_id<strmap>()>;
    CPPA_CHECK_EQUAL(detail::is_iterable<strmap>::value, true);
    CPPA_CHECK_EQUAL(detail::is_stl_compliant_list<vector<int>>::value, true);
    CPPA_CHECK_EQUAL(detail::is_stl_compliant_list<strmap>::value, false);
    CPPA_CHECK_EQUAL(detail::is_stl_compliant_map<strmap>::value, true);
    CPPA_CHECK_EQUAL(detail::impl_id<strmap>(), 2);
    CPPA_CHECK_EQUAL(token::value, 2);

    announce(typeid(raw_struct),
             uniform_type_info_ptr{new raw_struct_type_info});

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
          CPPA_CHECK(ok);
      }
      catch (std::exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

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
          CPPA_CHECK_EQUAL(get<0>(tup0), get<0>(tref));
          CPPA_CHECK_EQUAL(get<1>(tup0), get<1>(tref));
      }
      catch (std::exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

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
          CPPA_CHECK_EQUAL(rs2.str, rs.str);
          auto rsstr = cppa::to_string(object::from(rs));
          rs2.str = "foobar";
          CPPA_PRINT("rs as string: " << rsstr);
          rs2 = from_string<raw_struct>(rsstr);
          CPPA_CHECK_EQUAL(rs2.str, rs.str);
      }
      catch (std::exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

      try {
          scoped_actor self;
          auto ttup = make_message(1, 2, actor{self.get()});
          io::buffer wr_buf;
          binary_serializer bs(&wr_buf, &addressing);
          bs << ttup;
          binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
          message ttup2;
          uniform_typeid<message>()->deserialize(&ttup2, &bd);
          CPPA_CHECK(ttup  == ttup2);
      }
      catch (std::exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

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
          CPPA_CHECK(ttup  == ttup2);
          CPPA_CHECK(ttup  == ttup3);
          CPPA_CHECK(ttup2 == ttup3);
      }
      catch (std::exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

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
          CPPA_CHECK(ok);
      }
      catch (std::exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

      CPPA_CHECK((is_iterable<int>::value) == false);
      // string is primitive and thus not identified by is_iterable
      CPPA_CHECK((is_iterable<string>::value) == false);
      CPPA_CHECK((is_iterable<list<int>>::value) == true);
      CPPA_CHECK((is_iterable<map<int, int>>::value) == true);
      try {  // test meta_object implementation for primitive types
          auto meta_int = uniform_typeid<uint32_t>();
          CPPA_CHECK(meta_int != nullptr);
          if (meta_int) {
              auto o = meta_int->create();
              get_ref<uint32_t>(o) = 42;
              auto str = to_string(object::from(get<uint32_t>(o)));
              CPPA_CHECK_EQUAL( "@u32 ( 42 )", str);
          }
      }
      catch (std::exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

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
          CPPA_CHECK(ok);
      }
      catch (std::exception& e) { CPPA_FAILURE(to_verbose_string(e)); }
    */
    return CPPA_TEST_RESULT();
}
