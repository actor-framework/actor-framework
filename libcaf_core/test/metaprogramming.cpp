/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE metaprogramming
#include "caf/test/unit_test.hpp"

#include <string>
#include <cstdint>
#include <typeinfo>
#include <type_traits>

#include "caf/all.hpp"

#include "caf/detail/ctm.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"

using namespace caf;
using namespace caf::detail;

template <class T>
struct is_int : std::false_type {};

template <>
struct is_int<int> : std::true_type {};

CAF_TEST(metaprogramming) {
  using std::is_same;
  using if1 = type_list<replies_to<int, double>::with<void>,
                        replies_to<int>::with<int>>;
  using if2 = type_list<replies_to<int>::with<int>,
                        replies_to<int, double>::with<void>>;
  using if3 = type_list<replies_to<int, double>::with<void>>;
  using if4 = type_list<replies_to<int>::with<skip_message_t>,
                        replies_to<int, double>::with<void>>;
  CAF_CHECK((ctm<if1, if2>::value) == -1);
  CAF_CHECK((ctm<if1, if3>::value) != -1);
  CAF_CHECK((ctm<if2, if3>::value) != -1);
  CAF_CHECK((ctm<if1, if4>::value) == -1);
  CAF_CHECK((ctm<if2, if4>::value) == -1);

  using l1 = type_list<int, float, std::string>;
  using r1 = tl_reverse<l1>::type;

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
  using il2 = il_right<il0, 2>::type;
  CAF_CHECK((is_same<il2, il1>::value));

  /* test tlf_is_subset */ {
    using list_a = type_list<int, float, double>;
    using list_b = type_list<float, int, double, std::string>;
    CAF_CHECK((tlf_is_subset(list_a{}, list_b{})));
    CAF_CHECK(! (tlf_is_subset(list_b{}, list_a{})));
    CAF_CHECK((tlf_is_subset(list_a{}, list_a{})));
    CAF_CHECK((tlf_is_subset(list_b{}, list_b{})));
  }
}
