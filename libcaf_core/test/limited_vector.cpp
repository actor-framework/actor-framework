/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#define CAF_SUITE limited_vector
#include "caf/test/unit_test.hpp"

#include <algorithm>

#include "caf/detail/limited_vector.hpp"

using caf::detail::limited_vector;

CAF_TEST(basics) {
  using std::equal;
  int arr1[] {1, 2, 3, 4};
  limited_vector<int, 4> vec1 {1, 2, 3, 4};
  limited_vector<int, 5> vec2 {4, 3, 2, 1};
  limited_vector<int, 4> vec3;
  for (int i = 1; i <= 4; ++i) vec3.push_back(i);
  limited_vector<int, 4> vec4 {1, 2};
  limited_vector<int, 2> vec5 {3, 4};
  vec4.insert(vec4.end(), vec5.begin(), vec5.end());
  auto vec6 = vec4;
  CAF_CHECK_EQUAL(vec1.size(), 4u);
  CAF_CHECK_EQUAL(vec2.size(), 4u);
  CAF_CHECK_EQUAL(vec3.size(), 4u);
  CAF_CHECK_EQUAL(vec4.size(), 4u);
  CAF_CHECK_EQUAL(vec5.size(), 2u);
  CAF_CHECK_EQUAL(vec6.size(), 4u);
  CAF_CHECK_EQUAL(vec1.full(), true);
  CAF_CHECK_EQUAL(vec2.full(), false);
  CAF_CHECK_EQUAL(vec3.full(), true);
  CAF_CHECK_EQUAL(vec4.full(), true);
  CAF_CHECK_EQUAL(vec5.full(), true);
  CAF_CHECK_EQUAL(vec6.full(), true);
  CAF_CHECK(std::equal(vec1.begin(), vec1.end(), arr1));
  CAF_CHECK(std::equal(vec2.rbegin(), vec2.rend(), arr1));
  CAF_CHECK(std::equal(vec4.begin(), vec4.end(), arr1));
  CAF_CHECK(std::equal(vec6.begin(), vec6.end(), arr1));
  CAF_CHECK(std::equal(vec6.begin(), vec6.end(), vec2.rbegin()));
  limited_vector<int, 10> vec7 {5, 9};
  limited_vector<int, 10> vec8 {1, 2, 3, 4};
  limited_vector<int, 10> vec9 {6, 7, 8};
  vec7.insert(vec7.begin() + 1, vec9.begin(), vec9.end());
  vec7.insert(vec7.begin(), vec8.begin(), vec8.end());
  CAF_CHECK_EQUAL(vec7.full(), false);
  limited_vector<int, 1> vec10 {10};
  vec7.insert(vec7.end(), vec10.begin(), vec10.end());
  CAF_CHECK_EQUAL(vec7.full(), true);
  CAF_CHECK((std::is_sorted(vec7.begin(), vec7.end())));
  int arr2[] {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  CAF_CHECK((std::equal(vec7.begin(), vec7.end(), std::begin(arr2))));
  vec7.assign(std::begin(arr2), std::end(arr2));
  CAF_CHECK((std::equal(vec7.begin(), vec7.end(), std::begin(arr2))));
  vec7.assign(5, 0);
  CAF_CHECK_EQUAL(vec7.size(), 5u);
  CAF_CHECK((std::all_of(vec7.begin(), vec7.end(),
              [](int i) { return i == 0; })));
}
