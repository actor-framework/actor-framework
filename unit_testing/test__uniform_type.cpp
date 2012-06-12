#include <map>
#include <set>
#include <memory>
#include <cctype>
#include <atomic>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <stdexcept>

#include "test.hpp"

#include "cppa/atom.hpp"
#include "cppa/announce.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/uniform_type_info.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/addressed_message.hpp"

#include "cppa/util/callable_trait.hpp"

using std::cout;
using std::endl;

namespace {

struct foo {
    int value;
    explicit foo(int val = 0) : value(val) { }
};

inline bool operator==(const foo& lhs, const foo& rhs) {
    return lhs.value == rhs.value;
}

} // namespace <anonymous>

using namespace cppa;

size_t test__uniform_type() {
    CPPA_TEST(test__uniform_type);
    bool announce1 = announce<foo>(&foo::value);
    bool announce2 = announce<foo>(&foo::value);
    bool announce3 = announce<foo>(&foo::value);
    bool announce4 = announce<foo>(&foo::value);
    {
        //bar.create_object();
        object obj1 = uniform_typeid<foo>()->create();
        object obj2(obj1);
        CPPA_CHECK(obj1 == obj2);
        get_ref<foo>(obj1).value = 42;
        CPPA_CHECK(obj1 != obj2);
        CPPA_CHECK_EQUAL(get<foo>(obj1).value, 42);
        CPPA_CHECK_EQUAL(get<foo>(obj2).value, 0);
    }
    auto int_val = [](bool val) { return val ? 1 : 0; };
    int successful_announces =   int_val(announce1)
                               + int_val(announce2)
                               + int_val(announce3)
                               + int_val(announce4);
    CPPA_CHECK_EQUAL(successful_announces, 1);
    // these types (and only those) are present if
    // the uniform_type_info implementation is correct
    std::set<std::string> expected = {
        "@_::foo",                      // <anonymous namespace>::foo
        "@i8", "@i16", "@i32", "@i64",  // signed integer names
        "@u8", "@u16", "@u32", "@u64",  // unsigned integer names
        "@str", "@u16str", "@u32str",   // strings
        "float", "double",              // floating points
        "@0",                           // cppa::util::void_type
        // default announced cppa types
        "@atom",               // cppa::atom_value
        "@<>",                 // cppa::any_tuple
        "@msg",                // cppa::detail::addressed_message
        "@actor",              // cppa::actor_ptr
        "@group",              // cppa::group_ptr
        "@channel",            // cppa::channel_ptr
        "@process_info",       // cppa::intrusive_ptr<cppa::process_information>
        "cppa::util::duration"
    };
    // holds the type names we see at runtime
    std::set<std::string> found;
    // fetch all available type names
    auto types = uniform_type_info::instances();
    for (auto tinfo : types) {
        found.insert(tinfo->name());
    }
    // compare the two sets
    CPPA_CHECK_EQUAL(expected.size(), found.size());
    bool expected_equals_found = false;
    if (expected.size() == found.size()) {
        expected_equals_found = std::equal(found.begin(),
                                           found.end(),
                                           expected.begin());
        CPPA_CHECK(expected_equals_found);
    }
    if (!expected_equals_found) {
        cout << "found:" << endl;
        for (const std::string& tname : found) {
            cout << "    - " << tname << endl;
        }
        cout << "expected: " << endl;
        for (const std::string& tname : expected) {
            cout << "    - " << tname << endl;
        }
    }

    // check if static types are identical to runtime types
    auto& sarr = detail::static_types_array<
                    std::int8_t, std::int16_t, std::int32_t, std::int64_t,
                    std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t,
                    std::string, std::u16string, std::u32string,
                    float, double,
                    atom_value, any_tuple, detail::addressed_message,
                    actor_ptr, group_ptr,
                    channel_ptr, intrusive_ptr<process_information>
                 >::arr;

    std::vector<const uniform_type_info*> rarr{
        uniform_typeid<std::int8_t>(),
        uniform_typeid<std::int16_t>(),
        uniform_typeid<std::int32_t>(),
        uniform_typeid<std::int64_t>(),
        uniform_typeid<std::uint8_t>(),
        uniform_typeid<std::uint16_t>(),
        uniform_typeid<std::uint32_t>(),
        uniform_typeid<std::uint64_t>(),
        uniform_typeid<std::string>(),
        uniform_typeid<std::u16string>(),
        uniform_typeid<std::u32string>(),
        uniform_typeid<float>(),
        uniform_typeid<double>(),
        uniform_typeid<atom_value>(),
        uniform_typeid<any_tuple>(),
        uniform_typeid<detail::addressed_message>(),
        uniform_typeid<actor_ptr>(),
        uniform_typeid<group_ptr>(),
        uniform_typeid<channel_ptr>(),
        uniform_typeid<intrusive_ptr<process_information> >()
    };

    CPPA_CHECK_EQUAL(true, sarr.is_pure());

    for (size_t i = 0; i < sarr.size; ++i) {
        CPPA_CHECK_EQUAL(sarr[i]->name(), rarr[i]->name());
        CPPA_CHECK(sarr[i] == rarr[i]);
    }

    auto& arr0 = detail::static_types_array<atom_value, std::uint32_t>::arr;
    CPPA_CHECK(arr0.is_pure());
    CPPA_CHECK(arr0[0] == uniform_typeid<atom_value>());
    CPPA_CHECK(arr0[0] == uniform_type_info::from("@atom"));
    CPPA_CHECK(arr0[1] == uniform_typeid<std::uint32_t>());
    CPPA_CHECK(arr0[1] == uniform_type_info::from("@u32"));
    CPPA_CHECK(uniform_type_info::from("@u32") == uniform_typeid<std::uint32_t>());

    auto& arr1 = detail::static_types_array<std::string, std::int8_t>::arr;
    CPPA_CHECK(arr1[0] == uniform_typeid<std::string>());
    CPPA_CHECK(arr1[0] == uniform_type_info::from("@str"));
    CPPA_CHECK(arr1[1] == uniform_typeid<std::int8_t>());
    CPPA_CHECK(arr1[1] == uniform_type_info::from("@i8"));

    auto& arr2 = detail::static_types_array<std::uint8_t, std::int8_t>::arr;
    CPPA_CHECK(arr2[0] == uniform_typeid<std::uint8_t>());
    CPPA_CHECK(arr2[0] == uniform_type_info::from("@u8"));
    CPPA_CHECK(arr2[1] == uniform_typeid<std::int8_t>());
    CPPA_CHECK(arr2[1] == uniform_type_info::from("@i8"));

    auto& arr3 = detail::static_types_array<atom_value, std::uint16_t>::arr;
    CPPA_CHECK(arr3[0] == uniform_typeid<atom_value>());
    CPPA_CHECK(arr3[0] == uniform_type_info::from("@atom"));
    CPPA_CHECK(arr3[1] == uniform_typeid<std::uint16_t>());
    CPPA_CHECK(arr3[1] == uniform_type_info::from("@u16"));
    CPPA_CHECK(uniform_type_info::from("@u16") == uniform_typeid<std::uint16_t>());

    return CPPA_TEST_RESULT;
}
