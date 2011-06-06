#include <string>
#include <typeinfo>
#include <iostream>

#include "test.hpp"
#include "hash_of.hpp"
#include "cppa/util.hpp"

#include "cppa/cppa.hpp"

using std::cout;
using std::endl;

using namespace cppa;
using namespace cppa::util;

static constexpr auto s_foo = atom("abc");

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

    CPPA_TEST(test__atom);

    self() << make_tuple(atom("foo"), static_cast<std::uint32_t>(42));

    receive(on<atom("foo"), std::uint32_t>() >> [&](std::uint32_t value) {
        CPPA_CHECK_EQUAL(value, 42);
    });

/*
    foo<static_cast<atom_value_type>(atom("abc").value()), int, float>();

    cout << "a    = " << get_atom_value<'a'>::_(0) << endl;
    cout << "ab   = " << get_atom_value<'a', 'b'>::_(0) << endl;
    cout << "abc  = " << get_atom_value<'a', 'b', 'c'>::_(0) << endl;
    cout << "abcd = " << get_atom_value<'a', 'b', 'c', 'd'>::_(0) << endl;
    cout << "__exit = " << get_atom_value<'_','_','e','x','i','t'>::_(0) << endl;
    cout << "cppa:exit = " << get_atom_value<'c','p','p','a',':','e','x','i','t'>::_(0) << endl;
    cout << "cppa::exit = " << get_atom_value<'c','p','p','a',':',':','e','x','i','t'>::_(0) << endl;

    atom a1 = "cppa::exit";
    cout << "a1 = " << to_string(a1) << endl;
    atom a3 = "abc";
    cout << "to_string(a3) = " << to_string(a3) << endl;

    cout << "s_a3 = " << to_string(s_a3) << endl;

    cout << "a3.value() = " << a3.value() << endl;
    cout << "s_a1.value() = " << s_a1.value() << endl;
    cout << "s_foo = " << s_foo << endl;
*/

    return CPPA_TEST_RESULT;

}
