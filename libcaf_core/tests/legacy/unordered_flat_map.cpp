// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE unordered_flat_map

#include "caf/unordered_flat_map.hpp"

#include "core-test.hpp"

#include <string>
#include <utility>
#include <vector>

namespace caf {

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

} // namespace caf

using std::make_pair;
using std::pair;
using std::string;
using std::vector;

using caf::unordered_flat_map;

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

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(default_constructed) {
  // A default-constructed map must be empty, i.e., have size 0.
  CHECK_EQ(xs.empty(), true);
  CHECK_EQ(xs.size(), 0u);
  // The begin() and end() iterators must compare equal.
  CHECK_EQ(xs.begin(), xs.end());
  CHECK_EQ(xs.cbegin(), xs.begin());
  CHECK_EQ(xs.cend(), xs.end());
  CHECK_EQ(xs.cbegin(), xs.cend());
  CHECK_EQ(xs.rbegin(), xs.rend());
  // Calling begin() and end() on a const reference must return the same as
  // cbegin() and cend().
  const auto& cxs = xs;
  CHECK_EQ(cxs.begin(), xs.cbegin());
  CHECK_EQ(cxs.end(), xs.cend());
}

CAF_TEST(initializer_list_constructed) {
  unordered_flat_map<int, int> zs{{1, 10}, {2, 20}, {3, 30}, {4, 40}};
  CHECK_EQ(zs.size(), 4u);
  CHECK_EQ(zs, ivec({{1, 10}, {2, 20}, {3, 30}, {4, 40}}));
}

CAF_TEST(range_constructed) {
  kvp_vec<int> tmp{{1, 10}, {2, 20}, {3, 30}, {4, 40}};
  unordered_flat_map<int, int> zs(tmp.begin(), tmp.end());
  CHECK_EQ(zs.size(), 4u);
  CHECK_EQ(zs, tmp);
}

CAF_TEST(integer_insertion) {
  xs.insert(kvp(3, 30));
  xs.insert(xs.begin(), kvp(2, 20));
  xs.insert(xs.cbegin(), kvp(1, 10));
  xs.emplace(5, 50);
  xs.emplace_hint(xs.cend() - 1, 4, 40);
  CHECK_EQ(xs, ivec({{1, 10}, {2, 20}, {3, 30}, {4, 40}, {5, 50}}));
}

CAF_TEST(integer_removal) {
  fill_xs();
  CHECK_EQ(xs, ivec({{1, 10}, {2, 20}, {3, 30}, {4, 40}}));
  xs.erase(xs.begin());
  CHECK_EQ(xs, ivec({{2, 20}, {3, 30}, {4, 40}}));
  xs.erase(xs.begin(), xs.begin() + 2);
  CHECK_EQ(xs, ivec({{4, 40}}));
  xs.erase(4);
  CHECK_EQ(xs.empty(), true);
  CHECK_EQ(xs.size(), 0u);
}

CAF_TEST(lookup) {
  fill_xs();
  CHECK_EQ(xs.count(2), 1u);
  CHECK_EQ(xs.count(6), 0u);
  // trigger non-const member functions
  CHECK_EQ(xs.at(3), 30);
  CHECK_EQ(xs.find(1), xs.begin());
  CHECK_EQ(xs.find(2), xs.begin() + 1);
  // trigger const member functions
  const auto& cxs = xs;
  CHECK_EQ(cxs.at(2), 20);
  CHECK_EQ(cxs.find(4), xs.end() - 1);
  CHECK_EQ(cxs.find(5), xs.end());
}

#ifdef CAF_ENABLE_EXCEPTIONS
CAF_TEST(exceptions) {
  fill_xs();
  try {
    auto x = xs.at(10);
    CAF_FAIL("got an unexpected value: " << x);
  } catch (std::out_of_range&) {
    MESSAGE("got expected out_of_range exception");
  } catch (...) {
    CAF_FAIL("got an expected exception");
  }
}
#endif // CAF_ENABLE_EXCEPTIONS

// We repeat several tests with strings as value type instead of integers to
// trigger non-trivial destructors.

CAF_TEST(string_insertion) {
  ys.insert(kvp(3, "c"));
  ys.insert(ys.begin(), kvp(2, "b"));
  ys.insert(ys.cbegin(), kvp(1, "a"));
  ys.emplace(5, "e");
  ys.emplace_hint(ys.cend() - 1, 4, "d");
  kvp_vec<string> tmp{{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}, {5, "e"}};
  CHECK_EQ(ys, tmp);
}

CAF_TEST(string_removal) {
  fill_ys();
  CHECK_EQ(ys, svec({{1, "a"}, {2, "b"}, {3, "c"}, {4, "d"}}));
  ys.erase(ys.begin());
  CHECK_EQ(ys, svec({{2, "b"}, {3, "c"}, {4, "d"}}));
  ys.erase(ys.begin(), ys.begin() + 2);
  CHECK_EQ(ys, svec({{4, "d"}}));
  ys.erase(4);
  CHECK_EQ(ys.empty(), true);
  CHECK_EQ(ys.size(), 0u);
}

END_FIXTURE_SCOPE()
