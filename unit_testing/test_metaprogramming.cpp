#include <string>
#include <cstdint>
#include <typeinfo>
#include <type_traits>

#include "test.hpp"

#include "caf/uniform_type_info.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"

#include "caf/detail/demangle.hpp"

using std::cout;
using std::endl;
using std::is_same;

using namespace caf;
using namespace caf::detail;

template <class T>
struct is_int : std::false_type {};

template <>
struct is_int<int> : std::true_type {};

int main() {

  CAF_TEST(test_metaprogramming);

  using l1 = type_list<int, float, std::string>;
  using r1 = typename tl_reverse<l1>::type;

  CAF_CHECK((is_same<int, tl_at<l1, 0>::type>::value));
  CAF_CHECK((is_same<float, tl_at<l1, 1>::type>::value));
  CAF_CHECK((is_same<std::string, tl_at<l1, 2>::type>::value));

  CAF_CHECK_EQUAL(3, tl_size<l1>::value);
  CAF_CHECK_EQUAL(tl_size<r1>::value, tl_size<l1>::value);
  CAF_CHECK((is_same<tl_at<l1, 0>::type, tl_at<r1, 2>::type>::value));
  CAF_CHECK((is_same<tl_at<l1, 1>::type, tl_at<r1, 1>::type>::value));
  CAF_CHECK((is_same<tl_at<l1, 2>::type, tl_at<r1, 0>::type>::value));

  using l2 = tl_concat<type_list<int>, l1>::type;

  CAF_CHECK((is_same<int, tl_head<l2>::type>::value));
  CAF_CHECK((is_same<l1, tl_tail<l2>::type>::value));

  CAF_CHECK_EQUAL((detail::tl_count<l1, is_int>::value), 1);
  CAF_CHECK_EQUAL((detail::tl_count<l2, is_int>::value), 2);

  using il0 = int_list<0, 1, 2, 3, 4, 5>;
  using il1 = int_list<4, 5>;
  using il2 = typename il_right<il0, 2>::type;
  CAF_CHECK_VERBOSE((is_same<il2, il1>::value),
             "il_right<il0, 2> returned "
               << detail::demangle<il2>()
               << "expected: " << detail::demangle<il1>());

  /* test tl_is_strict_subset */ {
    using list_a = type_list<int, float, double>;
    using list_b = type_list<float, int, double, std::string>;
    CAF_CHECK((tl_is_strict_subset<list_a, list_b>::value));
    CAF_CHECK(!(tl_is_strict_subset<list_b, list_a>::value));
    CAF_CHECK((tl_is_strict_subset<list_a, list_a>::value));
    CAF_CHECK((tl_is_strict_subset<list_b, list_b>::value));
  }

  return CAF_TEST_RESULT();
}
