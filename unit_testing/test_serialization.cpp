#define CPPA_VERBOSE_CHECK

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

#include "cppa/self.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/announce.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/to_string.hpp"
#include "cppa/serializer.hpp"
#include "cppa/from_string.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/primitive_type.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/util/pt_token.hpp"
#include "cppa/util/is_iterable.hpp"
#include "cppa/util/is_primitive.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

#include "cppa/network/default_actor_addressing.hpp"

#include "cppa/detail/object_array.hpp"
#include "cppa/detail/type_to_ptype.hpp"
#include "cppa/detail/ptype_to_type.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::util;

using cppa::detail::type_to_ptype;
using cppa::detail::ptype_to_type;

namespace { const size_t ui32size = sizeof(uint32_t); }

struct struct_a {
    int x;
    int y;
};

bool operator==(const struct_a& lhs, const struct_a& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool operator!=(const struct_a& lhs, const struct_a& rhs) {
    return !(lhs == rhs);
}

struct struct_b {
    struct_a a;
    int z;
    list<int> ints;
};

string to_string(const struct_b& what) {
    return to_string(object::from(what));
}

bool operator==(const struct_b& lhs, const struct_b& rhs) {
    return lhs.a == rhs.a && lhs.z == rhs.z && lhs.ints == rhs.ints;
}

bool operator!=(const struct_b& lhs, const struct_b& rhs) {
    return !(lhs == rhs);
}

typedef map<string, u16string> strmap;

struct struct_c {
    strmap strings;
    set<int> ints;
};

bool operator==(const struct_c& lhs, const struct_c& rhs) {
    return lhs.strings == rhs.strings && lhs.ints == rhs.ints;
}

bool operator!=(const struct_c& lhs, const struct_c& rhs) {
    return !(lhs == rhs);
}

static const char* msg1str = u8R"__({ @i32 ( 42 ), "Hello \"World\"!" })__";

struct raw_struct {
    string str;
};

bool operator==(const raw_struct& lhs, const raw_struct& rhs) {
    return lhs.str == rhs.str;
}

bool operator!=(const raw_struct& lhs, const raw_struct& rhs) {
    return lhs.str != rhs.str;
}

struct raw_struct_type_info : util::abstract_uniform_type_info<raw_struct> {
    void serialize(const void* ptr, serializer* sink) const {
        auto rs = reinterpret_cast<const raw_struct*>(ptr);
        sink->begin_object(name());
        sink->write_value(static_cast<uint32_t>(rs->str.size()));
        sink->write_raw(rs->str.size(), rs->str.data());
        sink->end_object();
    }
    void deserialize(void* ptr, deserializer* source) const {
        assert_type_name(source);
        source->begin_object(name());
        auto rs = reinterpret_cast<raw_struct*>(ptr);
        rs->str.clear();
        auto size = source->read<std::uint32_t>();
        rs->str.resize(size);
        source->read_raw(size, &(rs->str[0]));
        source->end_object();
    }
};

int main() {
    CPPA_TEST(test_serialization);

    announce(typeid(raw_struct), new raw_struct_type_info);

    network::default_actor_addressing addressing;

    auto oarr = new detail::object_array;
    oarr->push_back(object::from(static_cast<uint32_t>(42)));
    oarr->push_back(object::from("foo"  ));

    any_tuple atuple1(oarr);
    try {
        auto opt = tuple_cast<uint32_t, string>(atuple1);
        CPPA_CHECK(opt.valid());
        if (opt) {
            auto& tup = *opt;
            CPPA_CHECK_EQUAL(tup.size(), static_cast<size_t>(2));
            CPPA_CHECK_EQUAL(get<0>(tup), static_cast<uint32_t>(42));
            CPPA_CHECK_EQUAL(get<1>(tup), "foo");
        }
    }
    catch (exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

    try {
        // test raw_type in both binary and string serialization
        raw_struct rs;
        rs.str = "Lorem ipsum dolor sit amet.";
        util::buffer wr_buf;
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
    catch (exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

    try {
        auto ttup = make_any_tuple(1, 2, actor_ptr(self));
        util::buffer wr_buf;
        binary_serializer bs(&wr_buf, &addressing);
        bs << ttup;
        bs << ttup;
        binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
        any_tuple ttup2;
        any_tuple ttup3;
        uniform_typeid<any_tuple>()->deserialize(&ttup2, &bd);
        uniform_typeid<any_tuple>()->deserialize(&ttup3, &bd);
        CPPA_CHECK(ttup  == ttup2);
        CPPA_CHECK(ttup  == ttup3);
        CPPA_CHECK(ttup2 == ttup3);
    }
    catch (exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

    try {
        // serialize b1 to buf
        util::buffer wr_buf;
        binary_serializer bs(&wr_buf, &addressing);
        bs << atuple1;
        // deserialize b2 from buf
        binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
        any_tuple atuple2;
        uniform_typeid<any_tuple>()->deserialize(&atuple2, &bd);
        auto opt = tuple_cast<uint32_t, string>(atuple2);
        CPPA_CHECK(opt.valid());
        if (opt.valid()) {
            auto& tup = *opt;
            CPPA_CHECK_EQUAL(tup.size(), 2);
            CPPA_CHECK_EQUAL(get<0>(tup), 42);
            CPPA_CHECK_EQUAL(get<1>(tup), "foo");
        }
    }
    catch (exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

    try {
        any_tuple msg1 = cppa::make_cow_tuple(42, string("Hello \"World\"!"));
        auto msg1_tostring = to_string(msg1);
        if (msg1str != msg1_tostring) {
            CPPA_FAILURE("msg1str != to_string(msg1)");
            cerr << "to_string(msg1) = " << msg1_tostring << endl;
            cerr << "to_string(msg1str) = " << msg1str << endl;
        }
        util::buffer wr_buf;
        binary_serializer bs(&wr_buf, &addressing);
        bs << msg1;
        binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
        object obj1;
        bd >> obj1;
        object obj2 = from_string(to_string(msg1));
        CPPA_CHECK(obj1 == obj2);
        if (typeid(any_tuple) == *(obj1.type()) && obj2.type() == obj1.type()) {
            auto& content1 = get<any_tuple>(obj1);
            auto& content2 = get<any_tuple>(obj2);
            auto opt1 = tuple_cast<decltype(42), string>(content1);
            auto opt2 = tuple_cast<decltype(42), string>(content2);
            CPPA_CHECK(opt1.valid() && opt2.valid());
            if (opt1.valid() && opt2.valid()) {
                auto& tup1 = *opt1;
                auto& tup2 = *opt2;
                CPPA_CHECK_EQUAL(tup1.size(), 2);
                CPPA_CHECK_EQUAL(tup2.size(), 2);
                CPPA_CHECK_EQUAL(get<0>(tup1), 42);
                CPPA_CHECK_EQUAL(get<0>(tup2), 42);
                CPPA_CHECK_EQUAL(get<1>(tup1), "Hello \"World\"!");
                CPPA_CHECK_EQUAL(get<1>(tup2), "Hello \"World\"!");
            }
        }
        else {
            CPPA_FAILURE("obj.type() != typeid(message)");
        }
    }
    catch (exception& e) { CPPA_FAILURE(to_verbose_string(e)); }

    CPPA_CHECK((is_iterable<int>::value) == false);
    // string is primitive and thus not identified by is_iterable
    CPPA_CHECK((is_iterable<string>::value) == false);
    CPPA_CHECK((is_iterable<list<int>>::value) == true);
    CPPA_CHECK((is_iterable<map<int,int>>::value) == true);
    { // test meta_object implementation for primitive types
        auto meta_int = uniform_typeid<uint32_t>();
        CPPA_CHECK(meta_int != nullptr);
        if (meta_int) {
            auto o = meta_int->create();
            get_ref<uint32_t>(o) = 42;
            auto str = to_string(object::from(get<uint32_t>(o)));
            CPPA_CHECK_EQUAL( "@u32 ( 42 )", str);
        }
    }
    { // test serializers / deserializers with struct_b
        // get meta object for struct_b
        announce<struct_b>(compound_member(&struct_b::a,
                                           &struct_a::x,
                                           &struct_a::y),
                           &struct_b::z,
                           &struct_b::ints);
        // testees
        struct_b b1 = { { 1, 2 }, 3, list<int>{ 4, 5, 6, 7, 8, 9, 10 } };
        struct_b b2;
        struct_b b3;
        // expected result of to_string(&b1, meta_b)
        auto b1str = "struct_b ( struct_a ( 1, 2 ), 3, "
                     "{ 4, 5, 6, 7, 8, 9, 10 } )";
        // verify
        CPPA_CHECK_EQUAL((to_string(b1)), b1str); {
            // serialize b1 to buf
            util::buffer wr_buf;
            binary_serializer bs(&wr_buf, &addressing);
            bs << b1;
            // deserialize b2 from buf
            binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
            object res;
            bd >> res;
            CPPA_CHECK_EQUAL(res.type()->name(), "struct_b");
            b2 = get<struct_b>(res);
        }
        // verify result of serialization / deserialization
        CPPA_CHECK(b1 == b2);
        CPPA_CHECK_EQUAL(to_string(b2), b1str);
        { // deserialize b3 from string
            object res = from_string(b1str);
            CPPA_CHECK_EQUAL(res.type()->name(), "struct_b");
            b3 = get<struct_b>(res);
        }
        CPPA_CHECK(b1 == b3);
    }
    { // test serializers / deserializers with struct_c
        // get meta type of struct_c and "announce"
        announce<struct_c>(&struct_c::strings, &struct_c::ints);
        // testees
        struct_c c1{strmap{{"abc", u"cba" }, { "x", u"y" }}, set<int>{9, 4, 5}};
        struct_c c2; {
            // serialize c1 to buf
            util::buffer wr_buf;
            binary_serializer bs(&wr_buf, &addressing);
            bs << c1;
            // serialize c2 from buf
            binary_deserializer bd(wr_buf.data(), wr_buf.size(), &addressing);
            object res;
            bd >> res;
            CPPA_CHECK_EQUAL(res.type()->name(), "struct_c");
            c2 = get<struct_c>(res);
        }
        // verify result of serialization / deserialization
        CPPA_CHECK(c1 == c2);
    }
    return CPPA_TEST_RESULT();
}
