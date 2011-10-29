#include <string>
#include <typeinfo>
#include <iostream>

#include "test.hpp"
#include "hash_of.hpp"
#include "cppa/util.hpp"

#include "cppa/cppa.hpp"

using std::cout;
using std::endl;
using std::string;

using namespace cppa;
using namespace cppa::util;

namespace { constexpr auto s_foo = atom("FooBar"); }

template<atom_value AtomValue, typename... Types>
void foo()
{
    cout << "foo(" << static_cast<std::uint64_t>(AtomValue)
                   << " = "
                   << to_string(AtomValue)
         << ")"
         << endl;
}

size_t test__atom()
{
    bool matched_pattern[3] = { false, false, false };
    CPPA_TEST(test__atom);
    CPPA_CHECK_EQUAL(to_string(s_foo), "FooBar");
    self() << make_tuple(atom("foo"), static_cast<std::uint32_t>(42))
           << make_tuple(atom(":Attach"), atom(":Baz"), "cstring")
           << make_tuple(atom("b"), atom("a"), atom("c"), 23.f)
           << make_tuple(atom("a"), atom("b"), atom("c"), 23.f);
    int i = 0;
    receive_while([&i]() { return ++i <= 3; })
    (
        on<atom("foo"), std::uint32_t>() >> [&](std::uint32_t value)
        {
            matched_pattern[0] = true;
            CPPA_CHECK_EQUAL(value, 42);
        },
        on<atom(":Attach"), atom(":Baz"), string>() >> [&](const string& str)
        {
            matched_pattern[1] = true;
            CPPA_CHECK_EQUAL(str, "cstring");
        },
        on<atom("a"), atom("b"), atom("c"), float>() >> [&](float value)
        {
            matched_pattern[2] = true;
            CPPA_CHECK_EQUAL(value, 23.f);
        }
    );
    CPPA_CHECK(matched_pattern[0] && matched_pattern[1] && matched_pattern[2]);
    any_tuple msg = receive();
    CPPA_CHECK((match<atom_value, atom_value, atom_value, float>(msg.content(), nullptr, atom("b"), atom("a"), atom("c"))));
    CPPA_CHECK(try_receive(msg) == false);
    return CPPA_TEST_RESULT;
}
