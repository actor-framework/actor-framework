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

#include "cppa/detail/object_array.hpp"
#include "cppa/detail/type_to_ptype.hpp"
#include "cppa/detail/ptype_to_type.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace cppa;
using namespace cppa::util;

using cppa::detail::type_to_ptype;
using cppa::detail::ptype_to_type;

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
    std::list<int> ints;
};

bool operator==(const struct_b& lhs, const struct_b& rhs) {
    return lhs.a == rhs.a && lhs.z == rhs.z && lhs.ints == rhs.ints;
}

bool operator!=(const struct_b& lhs, const struct_b& rhs) {
    return !(lhs == rhs);
}

typedef std::map<std::string, std::u16string> strmap;

struct struct_c {
    strmap strings;
    std::set<int> ints;
};

bool operator==(const struct_c& lhs, const struct_c& rhs) {
    return lhs.strings == rhs.strings && lhs.ints == rhs.ints;
}

bool operator!=(const struct_c& lhs, const struct_c& rhs) {
    return !(lhs == rhs);
}

static const char* msg1str = u8R"__(@<> ( { @i32 ( 42 ), @str ( "Hello \"World\"!" ) } ))__";

size_t test__serialization() {
    CPPA_TEST(test__serialization);

    auto oarr = new detail::object_array;
    oarr->push_back(object::from(static_cast<std::uint32_t>(42)));
    oarr->push_back(object::from("foo"  ));

    any_tuple atuple1(oarr);
    try {
        auto opt = tuple_cast<std::uint32_t, std::string>(atuple1);
        CPPA_CHECK(opt.valid());
        if (opt) {
            auto& tup = *opt;
            CPPA_CHECK_EQUAL(tup.size(), static_cast<size_t>(2));
            CPPA_CHECK_EQUAL(get<0>(tup), static_cast<std::uint32_t>(42));
            CPPA_CHECK_EQUAL(get<1>(tup), "foo");
        }
    }
    catch (std::exception& e) {
        CPPA_ERROR("exception: " << e.what());
    }
    {
        any_tuple ttup = make_cow_tuple(1, 2, actor_ptr(self));
        binary_serializer bs;
        bs << ttup;
        binary_deserializer bd(bs.data(), bs.size());
        any_tuple ttup2;
        uniform_typeid<any_tuple>()->deserialize(&ttup2, &bd);
        CPPA_CHECK(ttup == ttup2);
    }
    {
        // serialize b1 to buf
        binary_serializer bs;
        bs << atuple1;
        // deserialize b2 from buf
        binary_deserializer bd(bs.data(), bs.size());
        any_tuple atuple2;
        uniform_typeid<any_tuple>()->deserialize(&atuple2, &bd);
        try {
            auto opt = tuple_cast<std::uint32_t, std::string>(atuple2);
            CPPA_CHECK(opt.valid());
            if (opt.valid()) {
                auto& tup = *opt;
                CPPA_CHECK_EQUAL(tup.size(), 2);
                CPPA_CHECK_EQUAL(get<0>(tup), 42);
                CPPA_CHECK_EQUAL(get<1>(tup), "foo");
            }
        }
        catch (std::exception& e) {
            CPPA_ERROR("exception: " << e.what());
        }
    }
    {
        any_tuple msg1 = cppa::make_cow_tuple(42, std::string("Hello \"World\"!"));
        auto msg1_tostring = to_string(msg1);
        if (msg1str != msg1_tostring) {
            CPPA_ERROR("msg1str != to_string(msg1)");
            cerr << "to_string(msg1) = " << msg1_tostring << endl;
        }
        binary_serializer bs;
        bs << msg1;
        binary_deserializer bd(bs.data(), bs.size());
        object obj1;
        bd >> obj1;
        object obj2 = from_string(to_string(msg1));
        CPPA_CHECK(obj1 == obj2);
        if (typeid(any_tuple) == *(obj1.type()) && obj2.type() == obj1.type()) {
            auto& content1 = get<any_tuple>(obj1);
            auto& content2 = get<any_tuple>(obj2);
            auto opt1 = tuple_cast<decltype(42), std::string>(content1);
            auto opt2 = tuple_cast<decltype(42), std::string>(content2);
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
            CPPA_ERROR("obj.type() != typeid(message)");
        }
    }

    CPPA_CHECK((is_iterable<int>::value) == false);
    // std::string is primitive and thus not identified by is_iterable
    CPPA_CHECK((is_iterable<std::string>::value) == false);
    CPPA_CHECK((is_iterable<std::list<int>>::value) == true);
    CPPA_CHECK((is_iterable<std::map<int,int>>::value) == true);
    { // test meta_object implementation for primitive types
        auto meta_int = uniform_typeid<std::uint32_t>();
        CPPA_CHECK(meta_int != nullptr);
        if (meta_int) {
            auto o = meta_int->create();
            get_ref<std::uint32_t>(o) = 42;
            auto str = to_string(get<std::uint32_t>(o));
            CPPA_CHECK_EQUAL(str, "@u32 ( 42 )");
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
        struct_b b1 = { { 1, 2 }, 3, std::list<int>{ 4, 5, 6, 7, 8, 9, 10 } };
        struct_b b2;
        struct_b b3;
        // expected result of to_string(&b1, meta_b)
        auto b1str = "struct_b ( struct_a ( 1, 2 ), 3, "
                     "{ 4, 5, 6, 7, 8, 9, 10 } )";
        // verify
        CPPA_CHECK_EQUAL((to_string(b1)), b1str); {
            // serialize b1 to buf
            binary_serializer bs;
            bs << b1;
            // deserialize b2 from buf
            binary_deserializer bd(bs.data(), bs.size());
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
        struct_c c1{strmap{{"abc", u"cba" }, { "x", u"y" }}, std::set<int>{9, 4, 5}};
        struct_c c2; {
            // serialize c1 to buf
            binary_serializer bs;
            bs << c1;
            // serialize c2 from buf
            binary_deserializer bd(bs.data(), bs.size());
            object res;
            bd >> res;
            CPPA_CHECK_EQUAL(res.type()->name(), "struct_c");
            c2 = get<struct_c>(res);
        }
        // verify result of serialization / deserialization
        CPPA_CHECK(c1 == c2);
    }
    return CPPA_TEST_RESULT;
}
