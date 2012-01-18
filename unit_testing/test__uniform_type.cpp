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

#include "cppa/announce.hpp"
#include "cppa/serializer.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/disjunction.hpp"
#include "cppa/util/callable_trait.hpp"

using std::cout;
using std::endl;

namespace {

struct foo
{
    int value;
    explicit foo(int val = 0) : value(val) { }
};

inline bool operator==(const foo& lhs, const foo& rhs)
{
    return lhs.value == rhs.value;
}

inline bool operator!=(const foo& lhs, const foo& rhs)
{
    return !(lhs == rhs);
}

} // namespace <anonymous>

using namespace cppa;

size_t test__uniform_type()
{
    CPPA_TEST(test__uniform_type);
    bool announce1 = announce<foo>(&foo::value);
    bool announce2 = announce<foo>(&foo::value);
    bool announce3 = announce<foo>(&foo::value);
    bool announce4 = announce<foo>(&foo::value);
    {
        //bar.create_object();
        object obj1 = uniform_typeid<foo>()->create();
        object obj2(obj1);
        CPPA_CHECK_EQUAL(obj1, obj2);
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
    std::set<std::string> expected =
    {
        "@_::foo",                      // <anonymous namespace>::foo
        "@i8", "@i16", "@i32", "@i64",  // signed integer names
        "@u8", "@u16", "@u32", "@u64",  // unsigned integer names
        "@str", "@u16str", "@u32str",   // strings
        "float", "double",              // floating points
        "@0",                           // cppa::util::void_type
        // default announced cppa types
        "@atom",                        // cppa::atom_value
        "@<>",                          // cppa::any_tuple
        "@msg",                         // cppa::message
        "@actor",                       // cppa::actor_ptr
        "@group",                       // cppa::group_ptr
        "@channel",                     // cppa::channel_ptr
        "cppa::util::duration"
    };
    if (sizeof(double) != sizeof(long double))
    {
        // long double is only present if it's not an alias for double
        expected.insert("long double");
    }
    // holds the type names we see at runtime
    std::set<std::string> found;
    // fetch all available type names
    auto types = uniform_type_info::instances();
    for (uniform_type_info* tinfo : types)
    {
        found.insert(tinfo->name());
    }
    // compare the two sets
    CPPA_CHECK_EQUAL(expected.size(), found.size());
    bool expected_equals_found = false;
    if (expected.size() == found.size())
    {
        expected_equals_found = std::equal(found.begin(),
                                           found.end(),
                                           expected.begin());
        CPPA_CHECK(expected_equals_found);
    }
    if (!expected_equals_found)
    {
        cout << "found:" << endl;
        for (const std::string& tname : found)
        {
            cout << "    - " << tname << endl;
        }
        cout << "expected: " << endl;
        for (const std::string& tname : expected)
        {
            cout << "    - " << tname << endl;
        }
    }
    return CPPA_TEST_RESULT;
}
