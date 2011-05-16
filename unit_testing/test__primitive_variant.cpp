#include "test.hpp"

#include "cppa/primitive_variant.hpp"

using namespace cppa;

size_t test__primitive_variant()
{
    CPPA_TEST(test__primitive_variant);
    std::uint32_t forty_two = 42;
    primitive_variant v1(forty_two);
    primitive_variant v2(pt_uint32);
    // type checking
    CPPA_CHECK_EQUAL(v1.ptype(), pt_uint32);
    CPPA_CHECK_EQUAL(v2.ptype(), pt_uint32);
    get<std::uint32_t&>(v2) = forty_two;
    CPPA_CHECK_EQUAL(v1, v2);
    CPPA_CHECK_EQUAL(v1, forty_two);
    CPPA_CHECK_EQUAL(forty_two, v2);
    // type mismatch => unequal
    CPPA_CHECK(v2 != static_cast<std::int8_t>(forty_two));
    v1 = "Hello world";
    CPPA_CHECK_EQUAL(v1.ptype(), pt_u8string);
    v2 = "Hello";
    CPPA_CHECK_EQUAL(v2.ptype(), pt_u8string);
    get<std::string>(v2) += " world";
    CPPA_CHECK_EQUAL(v1, v2);
    v2 = u"Hello World";
    CPPA_CHECK(v1 != v2);

    return CPPA_TEST_RESULT;
}
