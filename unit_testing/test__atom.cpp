#include <string>
#include <typeinfo>
#include <iostream>

#include "test.hpp"

#include "cppa/cppa.hpp"

namespace cppa {
inline std::ostream& operator<<(std::ostream& out, const atom_value& a) {
    return (out << to_string(a));
}
}

using std::cout;
using std::endl;
using std::string;

using namespace cppa;
using namespace cppa::util;

namespace { constexpr auto s_foo = atom("FooBar"); }

template<atom_value AtomValue, typename... Types>
void foo() {
    cout << "foo(" << static_cast<std::uint64_t>(AtomValue)
                   << " = "
                   << to_string(AtomValue)
         << ")"
         << endl;
}

size_t test__atom() {
    bool matched_pattern[3] = { false, false, false };
    CPPA_TEST(test__atom);
    // check if there are leading bits that distinguish "zzz" and "000 "
    CPPA_CHECK_NOT_EQUAL(atom("zzz"), atom("000 "));
    // 'illegal' characters are mapped to whitespaces
    CPPA_CHECK_EQUAL(atom("   "), atom("@!?"));
    CPPA_CHECK_NOT_EQUAL(atom("abc"), atom(" abc"));
    // check to_string impl.
    CPPA_CHECK_EQUAL(to_string(s_foo), "FooBar");
    self << make_cow_tuple(atom("foo"), static_cast<std::uint32_t>(42))
         << make_cow_tuple(atom(":Attach"), atom(":Baz"), "cstring")
         << make_cow_tuple(atom("b"), atom("a"), atom("c"), 23.f)
         << make_cow_tuple(atom("a"), atom("b"), atom("c"), 23.f);
    int i = 0;
    receive_for(i, 3) (
        on<atom("foo"), std::uint32_t>() >> [&](std::uint32_t value) {
            matched_pattern[0] = true;
            CPPA_CHECK_EQUAL(value, 42);
        },
        on<atom(":Attach"), atom(":Baz"), string>() >> [&](const string& str) {
            matched_pattern[1] = true;
            CPPA_CHECK_EQUAL(str, "cstring");
        },
        on<atom("a"), atom("b"), atom("c"), float>() >> [&](float value) {
            matched_pattern[2] = true;
            CPPA_CHECK_EQUAL(value, 23.f);
        }
    );
    CPPA_CHECK(matched_pattern[0] && matched_pattern[1] && matched_pattern[2]);
    receive (
        others() >> []() {
            // "erase" message { atom("b"), atom("a"), atom("c"), 23.f }
        }
    );
    return CPPA_TEST_RESULT;
}
