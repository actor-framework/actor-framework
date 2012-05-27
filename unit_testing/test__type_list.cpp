#include <string>
#include <cstdint>
#include <typeinfo>
#include <type_traits>

#include "test.hpp"

#include "cppa/util/at.hpp"
#include "cppa/util/element_at.hpp"
#include "cppa/uniform_type_info.hpp"

using std::cout;
using std::endl;
using std::is_same;

using namespace cppa::util;

size_t test__type_list() {

    CPPA_TEST(test__type_list);

    typedef type_list<int, float, std::string> l1;
    typedef typename tl_reverse<l1>::type r1;

    CPPA_CHECK((is_same<int, element_at<0, l1>::type>::value));
    CPPA_CHECK((is_same<float, element_at<1, l1>::type>::value));
    CPPA_CHECK((is_same<std::string, element_at<2, l1>::type>::value));

    CPPA_CHECK(l1::size == 3);
    CPPA_CHECK(l1::size == r1::size);
    CPPA_CHECK((is_same<element_at<0, l1>::type, element_at<2, r1>::type>::value));
    CPPA_CHECK((is_same<element_at<1, l1>::type, element_at<1, r1>::type>::value));
    CPPA_CHECK((is_same<element_at<2, l1>::type, element_at<0, r1>::type>::value));

    typedef tl_concat<type_list<int>, l1>::type l2;

    CPPA_CHECK((is_same<int, l2::head>::value));
    CPPA_CHECK((is_same<l1, l2::tail>::value));

    return CPPA_TEST_RESULT;

}
