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

#include "caf/atom.hpp"
#include "caf/announce.hpp"
#include "caf/message.hpp"
#include "caf/serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/detail/type_traits.hpp"

using std::cout;
using std::endl;

namespace {

struct foo {
    int value;
    explicit foo(int val = 0) : value(val) {}

};

inline bool operator==(const foo& lhs, const foo& rhs) {
    return lhs.value == rhs.value;
}

} // namespace <anonymous>

using namespace caf;

int main() {
    CAF_TEST(test_uniform_type);
    auto announce1 = announce<foo>(&foo::value);
    auto announce2 = announce<foo>(&foo::value);
    auto announce3 = announce<foo>(&foo::value);
    auto announce4 = announce<foo>(&foo::value);
    CAF_CHECK(announce1 == announce2);
    CAF_CHECK(announce1 == announce3);
    CAF_CHECK(announce1 == announce4);
    CAF_CHECK_EQUAL(announce1->name(), "$::foo");
    {
        /*
        //bar.create_object();
        auto obj1 = uniform_typeid<foo>()->create();
        auto obj2(obj1);
        CAF_CHECK(obj1 == obj2);
        get_ref<foo>(obj1).value = 42;
        CAF_CHECK(obj1 != obj2);
        CAF_CHECK_EQUAL(get<foo>(obj1).value, 42);
        CAF_CHECK_EQUAL(get<foo>(obj2).value, 0);
        */
    }
    {
        auto uti = uniform_typeid<atom_value>();
        CAF_CHECK(uti != nullptr);
        CAF_CHECK_EQUAL(uti->name(), "@atom");
    }
    // these types (and only those) are present if
    // the uniform_type_info implementation is correct
    std::set<std::string> expected = {
        // local types
        "$::foo", // <anonymous namespace>::foo
        // primitive types
        "bool",               "@i8",     "@i16",
        "@i32",               "@i64", // signed integer names
        "@u8",                "@u16",    "@u32",
        "@u64",                                      // unsigned integer names
        "@str",               "@u16str", "@u32str",  // strings
        "float",              "double",  "@ldouble", // floating points
        // default announced types
        "@unit",              // unit_t
        "@accept",            // accept_handle
        "@acceptor_closed",   // acceptor_closed_msg
        "@actor",             // actor
        "@addr",              // actor_addr
        "@atom",              // atom_value
        "@channel",           // channel
        "@charbuf",           // vector<char>
        "@connection",        // connection_handle
        "@connection_closed", // connection_closed_msg
        "@down",              // down_msg
        "@duration",          // duration
        "@exit",              // exit_msg
        "@group",             // group
        "@group_down",        // group_down_msg
        "@message",           // message
        "@message_id",        // message_id
        "@new_connection",    // new_connection_msg
        "@new_data",          // new_data_msg
        "@node",              // node_id
        "@strmap",            // map<string,string>
        "@sync_exited",       // sync_exited_msg
        "@sync_timeout",      // sync_timeout_msg
        "@timeout"            // timeout_msg
    };
    // holds the type names we see at runtime
    std::set<std::string> found;
    // fetch all available type names
    auto types = uniform_type_info::instances();
    for (auto tinfo : types) {
        found.insert(tinfo->name());
    }
    // compare the two sets
    CAF_CHECK_EQUAL(expected.size(), found.size());
    bool expected_equals_found = false;
    if (expected.size() == found.size()) {
        expected_equals_found =
            std::equal(found.begin(), found.end(), expected.begin());
        CAF_CHECK(expected_equals_found);
    }
    if (!expected_equals_found) {
        std::string(41, ' ');
        std::ostringstream oss(std::string(41, ' '));
        oss.seekp(0);
        oss << "found (" << found.size() << ")";
        oss.seekp(22);
        oss << "expected (" << found.size() << ")";
        std::string lhs;
        std::string rhs;
        CAF_PRINT(oss.str());
        CAF_PRINT(std::string(41, '-'));
        auto fi = found.begin();
        auto fe = found.end();
        auto ei = expected.begin();
        auto ee = expected.end();
        while (fi != fe || ei != ee) {
            if (fi != fe)
                lhs = *fi++;
            else
                lhs.clear();
            if (ei != ee)
                rhs = *ei++;
            else
                rhs.clear();
            lhs.resize(20, ' ');
            CAF_PRINT(lhs << "| " << rhs);
        }
    }
    return CAF_TEST_RESULT();
}
