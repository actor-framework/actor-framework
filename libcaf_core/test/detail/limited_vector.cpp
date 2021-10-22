// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.limited_vector

#include "caf/detail/limited_vector.hpp"

#include "core-test.hpp"

#include <algorithm>

using caf::detail::limited_vector;

CAF_TEST(basics) {
  using std::equal;
  int arr1[]{1, 2, 3, 4};
  limited_vector<int, 4> vec1{1, 2, 3, 4};
  limited_vector<int, 5> vec2{4, 3, 2, 1};
  limited_vector<int, 4> vec3;
  for (int i = 1; i <= 4; ++i)
    vec3.push_back(i);
  limited_vector<int, 4> vec4{1, 2};
  limited_vector<int, 2> vec5{3, 4};
  vec4.insert(vec4.end(), vec5.begin(), vec5.end());
  auto vec6 = vec4;
  CHECK_EQ(vec1.size(), 4u);
  CHECK_EQ(vec2.size(), 4u);
  CHECK_EQ(vec3.size(), 4u);
  CHECK_EQ(vec4.size(), 4u);
  CHECK_EQ(vec5.size(), 2u);
  CHECK_EQ(vec6.size(), 4u);
  CHECK_EQ(vec1.full(), true);
  CHECK_EQ(vec2.full(), false);
  CHECK_EQ(vec3.full(), true);
  CHECK_EQ(vec4.full(), true);
  CHECK_EQ(vec5.full(), true);
  CHECK_EQ(vec6.full(), true);
  CHECK(std::equal(vec1.begin(), vec1.end(), arr1));
  CHECK(std::equal(vec2.rbegin(), vec2.rend(), arr1));
  CHECK(std::equal(vec4.begin(), vec4.end(), arr1));
  CHECK(std::equal(vec6.begin(), vec6.end(), arr1));
  CHECK(std::equal(vec6.begin(), vec6.end(), vec2.rbegin()));
  limited_vector<int, 10> vec7{5, 9};
  limited_vector<int, 10> vec8{1, 2, 3, 4};
  limited_vector<int, 10> vec9{6, 7, 8};
  vec7.insert(vec7.begin() + 1, vec9.begin(), vec9.end());
  vec7.insert(vec7.begin(), vec8.begin(), vec8.end());
  CHECK_EQ(vec7.full(), false);
  limited_vector<int, 1> vec10{10};
  vec7.insert(vec7.end(), vec10.begin(), vec10.end());
  CHECK_EQ(vec7.full(), true);
  CHECK((std::is_sorted(vec7.begin(), vec7.end())));
  int arr2[]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  CHECK((std::equal(vec7.begin(), vec7.end(), std::begin(arr2))));
  vec7.assign(std::begin(arr2), std::end(arr2));
  CHECK((std::equal(vec7.begin(), vec7.end(), std::begin(arr2))));
  vec7.assign(5, 0);
  CHECK_EQ(vec7.size(), 5u);
  CHECK((std::all_of(vec7.begin(), vec7.end(), [](int i) { return i == 0; })));
}
