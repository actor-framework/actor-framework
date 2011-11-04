#include <string>
#include <typeinfo>

#include "test.hpp"
#include "cppa/util.hpp"
#include "cppa/anything.hpp"

using namespace cppa;
using namespace cppa::util;

size_t test__a_matches_b()
{
    CPPA_TEST(test__a_matches_b);

    /*
    typedef type_list<int, anything> int_star;
    typedef type_list<int, float, int> int_float_int;
    typedef type_list<int, int, std::string> int_int_string;
    typedef type_list<int, int, const std::string&> int_int_const_string_ref;

    typedef typename first_n<2, int_float_int>::type int_float;

    typedef type_list_apply<int_int_const_string_ref,
                            remove_const_reference>::type
            int_int_string2;

    CPPA_CHECK((std::is_same<int, remove_const_reference<const int&>::type>::value));

    CPPA_CHECK((a_matches_b<int_star, int_float_int>::value));

    CPPA_CHECK((a_matches_b<int_float_int, int_float_int>::value));
    CPPA_CHECK((a_matches_b<int_int_string, int_int_string>::value));
    CPPA_CHECK((a_matches_b<int_int_string, int_int_string2>::value));
    CPPA_CHECK(!(a_matches_b<int_int_string, int_int_const_string_ref>::value));

    CPPA_CHECK(!(a_matches_b<type_list<float>,
                             type_list<int, float, int>>::value));

    CPPA_CHECK((std::is_same<util::type_list<int, float>, int_float>::value));

    CPPA_CHECK((a_matches_b<type_list<anything, float>,
                            type_list<int, float, int>>::value) == false);

    CPPA_CHECK((a_matches_b<type_list<anything, float>,
                            type_list<int, int, float>>::value));
    */

    return CPPA_TEST_RESULT;

}
