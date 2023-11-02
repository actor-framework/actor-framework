// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/unordered_flat_map.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include <string>
#include <utility>
#include <vector>

using namespace caf;

namespace {

using int_map = unordered_flat_map<int, int>;

using string_map = unordered_flat_map<std::string, std::string>;

TEST("default constructed maps are empty") {
  int_map xs;
  // A default-constructed map must be empty, i.e., have size 0.
  check_eq(xs.empty(), true);
  check_eq(xs.size(), 0u);
  // The begin() and end() iterators must compare equal.
  check_eq(xs.begin(), xs.end());
  check_eq(xs.cbegin(), xs.cend());
  check_eq(xs.rbegin(), xs.rend());
  // Calling begin() and end() on a const reference must return the same as
  // cbegin() and cend().
  const auto& cxs = xs;
  check_eq(cxs.begin(), cxs.end());
  check_eq(cxs.rbegin(), cxs.rend());
}

TEST("reserve increases the capacity of the decorated container") {
  int_map xs;
  xs.reserve(10);
  check_ge(xs.container().capacity(), 10u);
  xs.emplace(1, 10);
  // Note: we call this here for coverage, but since this is a "non-binding
  // request" to the underlying container, we cannot check whether this actually
  // has an effect.
  xs.shrink_to_fit();
}

TEST("maps are constructible from initializer lists") {
  int_map xs{{1, 10}, {2, 20}, {3, 30}, {4, 40}};
  check_eq(xs.size(), 4u);
}

TEST("comparing two maps compares all elements") {
  int_map xs{{1, 10}, {2, 20}, {3, 30}, {4, 40}};
  check_eq(xs, int_map{{4, 40}, {2, 20}, {3, 30}, {1, 10}});
  check_ne(xs, int_map{{1, 10}, {2, 20}, {3, 30}});
  check_ne(xs, int_map{{1, 10}, {2, 20}, {3, 30}, {4, 44}});
  check_ne(xs, int_map{{1, 10}, {2, 20}, {3, 30}, {4, 40}, {5, 50}});
}

TEST("maps are constructible from iterator pairs") {
  std::vector<std::pair<int, int>> vec{{1, 10}, {2, 20}, {3, 30}, {4, 40}};
  int_map xs(vec.begin(), vec.end());
  check_eq(xs.size(), 4u);
}

TEST("insert and emplace add elements to the map if they are not present") {
  int_map xs;
  SECTION("insert single element") {
    check(xs.insert(std::pair{3, 30}).second);
    check_eq(xs.insert(xs.begin(), std::pair{2, 20})->second, 20);
    check_eq(xs.insert(xs.cbegin(), std::pair{1, 10})->second, 10);
    check(!xs.insert(std::pair{3, 90}).second);
  }
  SECTION("insert iterator range") {
    int_map ys{{1, 10}, {2, 20}, {3, 30}};
    xs.insert(ys.begin(), ys.end());
    check_eq(xs.size(), 3u);
    check_eq(xs[1], 10);
    check_eq(xs[2], 20);
    check_eq(xs[3], 30);
  }
  SECTION("emplace") {
    check(!xs.contains(5));
    check(xs.emplace(5, 50).second);
    check(xs.contains(5));
    check(!xs.emplace(5, 75).second);
  }
}

TEST("insert_or_assign inserts or overrides elements") {
  int_map xs;
  SECTION("insert single element") {
    check(xs.insert_or_assign(3, 30).second);
    check_eq(xs.size(), 1u);
    check(!xs.insert_or_assign(3, 90).second);
    if (check_eq(xs.size(), 1u)) {
      check_eq(xs.begin()->second, 90);
    }
  }
  SECTION("insert single element with hint") {
    check_eq(xs.insert_or_assign(xs.end(), 3, 30)->second, 30);
    check_eq(xs.size(), 1u);
    check_eq(xs.insert_or_assign(xs.begin(), 3, 90)->second, 90);
    if (check_eq(xs.size(), 1u)) {
      check_eq(xs.begin()->second, 90);
    }
  }
}

TEST("erase removes elements from a map") {
  int_map xs{{1, 10}, {2, 20}, {3, 30}, {4, 40}};
  SECTION("calling erase with an existing key removes the element") {
    check(xs.contains(3));
    check_eq(xs.size(), 4u);
    xs.erase(xs.find(3));
    check_eq(xs.size(), 3u);
    check(!xs.contains(3));
  }
  SECTION("calling erase with an iterator removes the element") {
    check_eq(xs.size(), 4u);
    check(xs.contains(3));
    xs.erase(xs.find(3));
    check_eq(xs.size(), 3u);
    check(!xs.contains(3));
  }
  SECTION("calling erase with an iterator range removes all of the elements") {
    xs.erase(xs.begin(), xs.end());
    check(xs.empty());
  }
  SECTION("calling erase with a non-existing key does nothing") {
    check_eq(xs.size(), 4u);
    check_eq(xs.erase(5), 0u);
    check_eq(xs.size(), 4u);
  }
}

TEST("element lookup") {
  int_map xs{{1, 10}, {2, 20}, {3, 30}, {4, 40}};
  SECTION("at() accesses existing elements") {
    check_eq(xs.at(1), 10);
    check_eq(xs.at(2), 20);
    check_eq(xs.at(3), 30);
    check_eq(xs.at(4), 40);
    xs.at(3) = 90;
    const auto& cxs = xs;
    check_eq(cxs.at(3), 90);
  }
  SECTION("operator[] accesses existing elements or inserts new ones") {
    check_eq(xs[1], 10);
    check_eq(xs[2], 20);
    check_eq(xs[3], 30);
    check_eq(xs[4], 40);
    check_eq(xs[5], 0);
  }
  SECTION("contains() returns true if the key is present") {
    check(xs.contains(1));
    check(xs.contains(2));
    check(xs.contains(3));
    check(xs.contains(4));
    check(!xs.contains(5));
  }
  SECTION("find() returns an iterator to the element if it is present") {
    check_eq(xs.find(1)->second, 10);
    check_eq(xs.find(2)->second, 20);
    check_eq(xs.find(3)->second, 30);
    check_eq(xs.find(4)->second, 40);
    check(xs.find(5) == xs.end());
    xs.find(3)->second = 90;
    const auto& cxs = xs;
    check_eq(cxs.find(3)->second, 90);
  }
  SECTION("count() returns 1 if the key is present and 0 otherwise") {
    check_eq(xs.count(1), 1u);
    check_eq(xs.count(2), 1u);
    check_eq(xs.count(3), 1u);
    check_eq(xs.count(4), 1u);
    check_eq(xs.count(5), 0u);
  }
}

#ifdef CAF_ENABLE_EXCEPTIONS
TEST("calling at() with an invalid key throws std::out_of_range") {
  int_map xs{{1, 10}, {2, 20}, {3, 30}, {4, 40}};
  check_throws<std::out_of_range>([&] { xs.at(10); });
}
#endif // CAF_ENABLE_EXCEPTIONS

} // namespace
