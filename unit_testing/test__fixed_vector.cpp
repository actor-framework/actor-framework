#include <string>
#include <typeinfo>
#include <iostream>
#include <algorithm>

#include "test.hpp"
#include "cppa/util/fixed_vector.hpp"

using std::cout;
using std::endl;
using std::equal;
using cppa::util::fixed_vector;

size_t test__fixed_vector()
{
    CPPA_TEST(test__fixed_vector);
    int arr1[] {1, 2, 3, 4};
    fixed_vector<int, 4> vec1 {1, 2, 3, 4};
    fixed_vector<int, 5> vec2 {4, 3, 2, 1};
    fixed_vector<int, 4> vec3;
    for (int i = 1; i <= 4; ++i) vec3.push_back(i);
    fixed_vector<int, 4> vec4 {1, 2};
    fixed_vector<int, 2> vec5 {3, 4};
    vec4.insert(vec4.end(), vec5.begin(), vec5.end());
    CPPA_CHECK_EQUAL(vec1.size(), 4);
    CPPA_CHECK_EQUAL(vec2.size(), 4);
    CPPA_CHECK_EQUAL(vec3.size(), 4);
    CPPA_CHECK_EQUAL(vec4.size(), 4);
    CPPA_CHECK_EQUAL(vec5.size(), 2);
    CPPA_CHECK_EQUAL(vec1.full(), true);
    CPPA_CHECK_EQUAL(vec2.full(), false);
    CPPA_CHECK_EQUAL(vec3.full(), true);
    CPPA_CHECK_EQUAL(vec4.full(), true);
    CPPA_CHECK_EQUAL(vec5.full(), true);
    CPPA_CHECK_EQUAL(vec4.full(), true);
    CPPA_CHECK(std::equal(vec1.begin(), vec1.end(), arr1));
    CPPA_CHECK(std::equal(vec2.rbegin(), vec2.rend(), arr1));
    CPPA_CHECK_EQUAL(vec4.size(), vec1.size());
    if (vec4.size() == vec1.size())
        CPPA_CHECK(std::equal(vec4.begin(), vec4.end(), arr1));
    return CPPA_TEST_RESULT;
}
