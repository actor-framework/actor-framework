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

static constexpr auto s_foo = atom("FooBar");

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

    CPPA_CHECK_EQUAL(to_string(s_foo), "FooBar");

    self() << make_tuple(atom("foo"), static_cast<std::uint32_t>(42));

    receive(on<atom("foo"), std::uint32_t>() >> [&](std::uint32_t value) {
        CPPA_CHECK_EQUAL(value, 42);
    });

    return CPPA_TEST_RESULT;

}
