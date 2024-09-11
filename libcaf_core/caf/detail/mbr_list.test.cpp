// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/mbr_list.hpp"

#include "caf/test/test.hpp"

#include <numeric>

using namespace caf;

namespace {

using list_type = detail::mbr_list<int>;

template <class... Ts>
void fill(list_type& xs, Ts... args) {
  (xs.emplace_back(args), ...);
}

} // namespace

TEST("a default default-constructed list is empty") {
  list_type uut;
  check_eq(uut.empty(), true);
  check_eq(uut.size(), 0u);
  check_eq(uut.begin(), uut.end());
}

TEST("lists are convertible to strings") {
  detail::monotonic_buffer_resource resource;
  list_type uut{&resource};
  check_eq(deep_to_string(uut), "[]");
  fill(uut, 1, 2, 3, 4);
  check_eq(uut.size(), 4ul);
  check_eq(deep_to_string(uut), "[1, 2, 3, 4]");
}

TEST("push_back adds elements to the back of the list") {
  detail::monotonic_buffer_resource resource;
  list_type uut{&resource};
  uut.push_back(1);
  uut.push_back(2);
  uut.push_back(3);
  check_eq(deep_to_string(uut), "[1, 2, 3]");
}

TEST("lists are movable") {
  detail::monotonic_buffer_resource resource;
  list_type uut{&resource};
  SECTION("move constructor") {
    fill(uut, 1, 2, 3);
    list_type q2 = std::move(uut);
    check_eq(uut.empty(), true); // NOLINT(bugprone-use-after-move)
    check_eq(q2.empty(), false);
    check_eq(deep_to_string(q2), "[1, 2, 3]");
  }
  SECTION("move assignment operator") {
    list_type q2{&resource};
    fill(q2, 1, 2, 3);
    uut = std::move(q2);
    check_eq(q2.empty(), true); // NOLINT(bugprone-use-after-move)
    check_eq(uut.empty(), false);
    check_eq(deep_to_string(uut), "[1, 2, 3]");
  }
}

TEST("the size of the list is the number of elements") {
  detail::monotonic_buffer_resource resource;
  list_type uut{&resource};
  fill(uut, 1, 2, 3);
  check_eq(uut.size(), 3u);
  fill(uut, 4, 5);
  check_eq(uut.size(), 5u);
}

TEST("lists allow iterator-based access") {
  detail::monotonic_buffer_resource resource;
  list_type uut{&resource};
  fill(uut, 1, 2, 3);
  // Mutable access via begin/end.
  for (auto& x : uut)
    x *= 2;
  check_eq(uut.front(), 2);
  check_eq(uut.back(), 6);
  // Immutable access via cbegin/cend.
  check_eq(std::accumulate(uut.cbegin(), uut.cend(), 0,
                           [](int acc, const int& x) { return acc + x; }),
           12);
}
