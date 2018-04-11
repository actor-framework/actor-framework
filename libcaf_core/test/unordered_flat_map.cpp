/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#define CAF_SUITE unordered_flat_map

#include "caf/test/unit_test.hpp"

#include <string>
#include <utility>
#include <vector>

#include "caf/detail/unordered_flat_map.hpp"

namespace caf {
namespace detail {

template <class T>
bool operator==(const unordered_flat_map<int, T>& xs,
                const std::vector<std::pair<int, T>>& ys) {
  return xs.container() == ys;
}

template <class T>
bool operator==(const std::vector<std::pair<int, T>>& xs,
                const unordered_flat_map<int, T>& ys) {
  return ys == xs;
}

} // namespace detail
} // namespace caf

using std::make_pair;
using std::pair;
using std::string;
using std::vector;

using caf::detail::unordered_flat_map;

using namespace caf;

template <class T>
using kvp_vec = vector<pair<int, T>>;

kvp_vec<int> ivec(std::initializer_list<std::pair<int, int>> xs) {
  return {xs};
}

kvp_vec<string> svec(std::initializer_list<std::pair<int, string>> xs) {
  return {xs};
}

namespace {

struct fixture {
  unordered_flat_map<int, int> xs;
  unordered_flat_map<int, string> ys;

  // fills xs with {1, 10} ... {4, 40}
  void fill_xs() {
    for (int i = 1; i < 5; ++i)
      xs.emplace(i, i * 10);
  }

  // fills xs with {1, "a"} ... {4, "d"}
  void fill_ys() {
    char buf[] = {'\0', '\0'};
    for (int i = 0; i < 4; ++i) {
      buf[0] = static_cast<char>('a' + i);
      ys.emplace(i + 1, buf);
    }
  }

  static pair<int, int> kvp(int x, int y) {
    return make_pair(x, y);
  }

  static pair<int, string> kvp(int x, string y) {
    return make_pair(x, std::move(y));
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(unordered_flat_map_tests, fixture)

CAF_TEST(default_constructed) {
  // A default-constructed map must be empty, i.e., have size 0.
  CAF_CHECK_EQUAL(xs.empty(), true);
  CAF_CHECK_EQUAL(xs.size(), 0u);
  // The begin() and end() iterators must compare equal.
  CAF_CHECK_EQUAL(xs.begin(), xs.end());
  CAF_CHECK_EQUAL(xs.cbegin(), xs.begin());
  CAF_CHECK_EQUAL(xs.cend(), xs.end());
  CAF_CHECK_EQUAL(xs.cbegin(), xs.cend());
  CAF_CHECK_EQUAL(xs.rbegin(), xs.rend());
  // Calling begin() and end() on a const reference must return the same as
  // cbegin() and cend().
  const auto& cxs = xs;
  CAF_CHECK_EQUAL(cxs.begin(), xs.cbegin());
  CAF_CHECK_EQUAL(cxs.end(), xs.cend());
}

CAF_TEST(initializer_list_constructed) {
  unordered_flat_map<int,  int> zs{{1, 10}, {2, 20}, {3, 30}, {4, 40}};
  CAF_CHECK_EQUAL(zs.size(), 4u);
  CAF_CHECK_EQUAL(zs, ivec({{1, 10}, {2, 20}, {3, 30}, {4, 40}}));
}

CAF_TEST(range_constructed) {
  kvp_vec<int> tmp{{1, 10}, {2, 20}, {3, 30}, {4, 40}};
  unordered_flat_map<int,  int> zs(tmp.begin(), tmp.end());
  CAF_CHECK_EQUAL(zs.size(), 4u);
  CAF_CHECK_EQUAL(zs, tmp);
}

CAF_TEST(integer_insertion) {
  xs.insert(kvp(3, 30));
  xs.insert(xs.begin(), kvp(2, 20));
  xs.insert(xs.cbegin(), kvp(1, 10));
  xs.emplace(5, 50);
  xs.emplace_hint(xs.cend() - 1, 4, 40);
  CAF_CHECK_EQUAL(xs, ivec({{1, 10}, {2, 20}, {3, 30}, {4, 40}, {5, 50}}));
}

CAF_TEST(integer_removal) {
  fill_xs();
  CAF_CHECK_EQUAL(xs, ivec({{1, 10}, {2, 20}, {3, 30}, {4, 40}}));
  xs.erase(xs.begin());
  CAF_CHECK_EQUAL(xs, ivec({{2, 20}, {3, 30}, {4, 40}}));
  xs.erase(xs.begin(), xs.begin() + 2);
  CAF_CHECK_EQUAL(xs, ivec({{4, 40}}));
  xs.erase(4);
  CAF_CHECK_EQUAL(xs.empty(), true);
  CAF_CHECK_EQUAL(xs.size(), 0u);
}

CAF_TEST(lookup) {
  fill_xs();
  CAF_CHECK_EQUAL(xs.count(2), 1u);
  CAF_CHECK_EQUAL(xs.count(6), 0u);
  // trigger non-const member functions
  CAF_CHECK_EQUAL(xs.at(3), 30);
  CAF_CHECK_EQUAL(xs.find(1), xs.begin());
  CAF_CHECK_EQUAL(xs.find(2), xs.begin() + 1);
  // trigger const member functions
  const auto& cxs = xs;
  CAF_CHECK_EQUAL(cxs.at(2), 20);
  CAF_CHECK_EQUAL(cxs.find(4), xs.end() - 1);
  CAF_CHECK_EQUAL(cxs.find(5), xs.end());
}

#ifndef CAF_NO_EXCEPTIONS
CAF_TEST(exceptions) {
  fill_xs();
  try {
    auto x = xs.at(10);
    CAF_FAIL("got an unexpected value: " << x);
  }
  catch (std::out_of_range&) {
    CAF_MESSAGE("got expected out_of_range exception");
  }
  catch (...) {
    CAF_FAIL("got an expected exception");
  }
}
#endif // CAF_NO_EXCEPTIONS

// We repeat several tests with strings as value type instead of integers to
// trigger non-trivial destructors.

CAF_TEST(string_insertion) {
  ys.insert(kvp(3, "c"));
  ys.insert(ys.begin(), kvp(2, "b"));
  ys.insert(ys.cbegin(), kvp(1, "a"));
  ys.emplace(5, "e");
  ys.emplace_hint(ys.cend() - 1, 4, "d");
  kvp_vec<string> tmp{{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}, {5, "e"}};
  CAF_CHECK_EQUAL(ys, tmp);
}

CAF_TEST(string_removal) {
  fill_ys();
  CAF_CHECK_EQUAL(ys, svec({{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}}));
  ys.erase(ys.begin());
  CAF_CHECK_EQUAL(ys, svec({{2, "b"}, {3, "c"}, {4, "d"}}));
  ys.erase(ys.begin(), ys.begin() + 2);
  CAF_CHECK_EQUAL(ys, svec({{4, "d"}}));
  ys.erase(4);
  CAF_CHECK_EQUAL(ys.empty(), true);
  CAF_CHECK_EQUAL(ys.size(), 0u);
}

CAF_TEST_FIXTURE_SCOPE_END()
