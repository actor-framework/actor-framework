#include <string>
#include <cstdint>
#include <typeinfo>
#include <type_traits>

#include "test.hpp"

#include "cppa/uniform_type_info.hpp"

#include "cppa/util/int_list.hpp"
#include "cppa/util/type_list.hpp"

#include "cppa/detail/demangle.hpp"

using std::cout;
using std::endl;
using std::is_same;

using namespace cppa;
using namespace cppa::util;

template<typename T>
struct is_int : std::false_type { };

template<>
struct is_int<int> : std::true_type { };

int main() {

    CPPA_TEST(test_metaprogramming);

    CPPA_PRINT("test type_list");

    typedef type_list<int, float, std::string> l1;
    typedef typename tl_reverse<l1>::type r1;

    CPPA_CHECK((is_same<int, tl_at<l1, 0>::type>::value));
    CPPA_CHECK((is_same<float, tl_at<l1, 1>::type>::value));
    CPPA_CHECK((is_same<std::string, tl_at<l1, 2>::type>::value));

    CPPA_CHECK_EQUAL(3 , tl_size<l1>::value);
    CPPA_CHECK_EQUAL(tl_size<r1>::value, tl_size<l1>::value);
    CPPA_CHECK((is_same<tl_at<l1, 0>::type, tl_at<r1, 2>::type>::value));
    CPPA_CHECK((is_same<tl_at<l1, 1>::type, tl_at<r1, 1>::type>::value));
    CPPA_CHECK((is_same<tl_at<l1, 2>::type, tl_at<r1, 0>::type>::value));

    typedef tl_concat<type_list<int>, l1>::type l2;

    CPPA_CHECK((is_same<int, tl_head<l2>::type>::value));
    CPPA_CHECK((is_same<l1, tl_tail<l2>::type>::value));

    CPPA_CHECK_EQUAL((util::tl_count<l1, is_int>::value), 1);
    CPPA_CHECK_EQUAL((util::tl_count<l2, is_int>::value), 2);

    CPPA_PRINT("test int_list");

    typedef int_list<0, 1, 2, 3, 4, 5> il0;
    typedef int_list<4, 5> il1;
    typedef typename il_right<il0, 2>::type il2;
    CPPA_CHECK_VERBOSE((is_same<il2, il1>::value),
                       "il_right<il0, 2> returned " <<detail::demangle<il2>()
                       << "expected: " << detail::demangle<il1>());

    /* test tl_is_strict_subset */ {
        typedef type_list<int,float,double> list_a;
        typedef type_list<float,int,double,std::string> list_b;
        CPPA_CHECK((tl_is_strict_subset<list_a, list_b>::value));
        CPPA_CHECK(!(tl_is_strict_subset<list_b, list_a>::value));
        CPPA_CHECK((tl_is_strict_subset<list_a, list_a>::value));
    }

    return CPPA_TEST_RESULT();
}
