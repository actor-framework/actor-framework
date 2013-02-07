#include <string>
#include <cstdint>
#include <typeinfo>
#include <type_traits>

#include "test.hpp"

#include "cppa/uniform_type_info.hpp"

#include "cppa/util/at.hpp"
#include "cppa/util/int_list.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/element_at.hpp"

#include "cppa/detail/demangle.hpp"

using std::cout;
using std::endl;
using std::is_same;

using namespace cppa;
using namespace cppa::util;

int main() {

    CPPA_TEST(test__metaprogramming);

    cout << "test type_list" << endl;

    typedef type_list<int,float,std::string> l1;
    typedef typename tl_reverse<l1>::type r1;

    CPPA_CHECK((is_same<int,element_at<0,l1>::type>::value));
    CPPA_CHECK((is_same<float,element_at<1,l1>::type>::value));
    CPPA_CHECK((is_same<std::string,element_at<2,l1>::type>::value));

    CPPA_CHECK_EQUAL(3 ,tl_size<l1>::value);
    CPPA_CHECK_EQUAL(tl_size<l1>::value, tl_size<r1>::value);
    CPPA_CHECK((is_same<element_at<0,l1>::type,element_at<2,r1>::type>::value));
    CPPA_CHECK((is_same<element_at<1,l1>::type,element_at<1,r1>::type>::value));
    CPPA_CHECK((is_same<element_at<2,l1>::type,element_at<0,r1>::type>::value));

    typedef tl_concat<type_list<int>,l1>::type l2;

    CPPA_CHECK((is_same<int,tl_head<l2>::type>::value));
    CPPA_CHECK((is_same<l1,tl_tail<l2>::type>::value));


    cout << "test int_list" << endl;

    typedef int_list<0,1,2,3,4,5> il0;
    typedef int_list<4,5> il1;
    typedef typename il_right<il0,2>::type il2;
    CPPA_CHECK_VERBOSE((is_same<il2,il1>::value),
                       "il_right<il0,2> returned " <<detail::demangle<il2>()
                       << "expected: " << detail::demangle<il1>());

    return CPPA_TEST_RESULT();
}
