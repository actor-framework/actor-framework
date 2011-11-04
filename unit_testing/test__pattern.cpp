#include <string>

#include "test.hpp"

#include "cppa/atom.hpp"
#include "cppa/tuple.hpp"
#include "cppa/pattern.hpp"

using namespace cppa;

size_t test__pattern()
{
    CPPA_TEST(test__pattern);

    auto x = make_tuple(atom("FooBar"), 42, "hello world");

    pattern<atom_value, int, std::string> p0;
    pattern<atom_value, int, std::string> p1(atom("FooBar"));
    pattern<atom_value, int, std::string> p2(atom("FooBar"), 42);
    pattern<atom_value, int, std::string> p3(atom("FooBar"), 42, "hello world");
    pattern<atom_value, anything, std::string> p4(atom("FooBar"), anything(), "hello world");
    pattern<atom_value, anything> p5(atom("FooBar"));
    pattern<anything> p6;
    pattern<atom_value, anything> p7;
    pattern<atom_value, anything, std::string> p8;

    CPPA_CHECK(p0(x));
    CPPA_CHECK(p1(x));
    CPPA_CHECK(p2(x));
    CPPA_CHECK(p3(x));
    CPPA_CHECK(p4(x));
    CPPA_CHECK(p5(x));
    CPPA_CHECK(p6(x));
    CPPA_CHECK(p7(x));
    CPPA_CHECK(p8(x));

    return CPPA_TEST_RESULT;
}
