// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/dictionary.hpp"

#include "caf/test/test.hpp"

using namespace caf;

using int_dict = dictionary<int>;

TEST("construction and comparison") {
  int_dict xs;
  check_eq(xs.empty(), true);
  check_eq(xs.size(), 0u);
  int_dict ys{{"foo", 1}, {"bar", 2}};
  check_eq(ys.empty(), false);
  check_eq(ys.size(), 2u);
  check_ne(xs, ys);
  int_dict zs{ys.begin(), ys.end()};
  check_eq(zs.empty(), false);
  check_eq(zs.size(), 2u);
  check_eq(ys, zs);
  zs.clear();
  check_eq(zs.empty(), true);
  check_eq(zs.size(), 0u);
  check_eq(xs, zs);
}

TEST("iterators") {
  using std::equal;
  using vector_type = std::vector<int_dict::value_type>;
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}};
  vector_type ys{{"a", 1}, {"b", 2}, {"c", 3}};
  check(equal(xs.begin(), xs.end(), ys.begin()));
  check(equal(xs.cbegin(), xs.cend(), ys.cbegin()));
  check(equal(xs.rbegin(), xs.rend(), ys.rbegin()));
  check(equal(xs.crbegin(), xs.crend(), ys.crbegin()));
}

TEST("swapping") {
  int_dict xs{{"foo", 1}, {"bar", 2}};
  int_dict ys;
  int_dict zs{{"foo", 1}, {"bar", 2}};
  check_ne(xs, ys);
  check_ne(ys, zs);
  check_eq(xs, zs);
  xs.swap(ys);
  check_ne(xs, ys);
  check_eq(ys, zs);
  check_ne(xs, zs);
}

TEST("emplacing") {
  int_dict xs;
  check_eq(xs.emplace("x", 1).second, true);
  check_eq(xs.emplace("y", 2).second, true);
  check_eq(xs.emplace("y", 3).second, false);
}

TEST("insertion") {
  int_dict xs;
  check_eq(xs.insert("a", 1).second, true);
  check_eq(xs.insert("b", 2).second, true);
  check_eq(xs.insert("c", 3).second, true);
  check_eq(xs.insert("c", 4).second, false);
  int_dict ys;
  check_eq(ys.insert_or_assign("a", 1).second, true);
  check_eq(ys.insert_or_assign("b", 2).second, true);
  check_eq(ys.insert_or_assign("c", 0).second, true);
  check_eq(ys.insert_or_assign("c", 3).second, false);
  check_eq(xs, ys);
}

TEST("insertion with hint") {
  int_dict xs;
  auto xs_last = xs.end();
  auto xs_insert = [&](std::string_view key, int val) {
    xs_last = xs.insert(xs_last, key, val);
  };
  xs_insert("a", 1);
  xs_insert("c", 3);
  xs_insert("b", 2);
  xs_insert("c", 4);
  int_dict ys;
  auto ys_last = ys.end();
  auto ys_insert_or_assign = [&](std::string_view key, int val) {
    ys_last = ys.insert_or_assign(ys_last, key, val);
  };
  ys_insert_or_assign("a", 1);
  ys_insert_or_assign("c", 0);
  ys_insert_or_assign("b", 2);
  ys_insert_or_assign("c", 3);
  check_eq(xs, ys);
}

TEST("bounds") {
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};
  const int_dict& const_xs = xs;
  check_eq(xs.lower_bound("c")->first, "c");
  check_eq(xs.upper_bound("c")->first, "d");
  check_eq(const_xs.lower_bound("c")->first, "c");
  check_eq(const_xs.upper_bound("c")->first, "d");
}

TEST("find") {
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};
  const int_dict& const_xs = xs;
  check_eq(xs.find("e"), xs.end());
  check_eq(xs.find("a")->second, 1);
  check_eq(xs.find("c")->second, 3);
  check_eq(const_xs.find("e"), xs.end());
  check_eq(const_xs.find("a")->second, 1);
  check_eq(const_xs.find("c")->second, 3);
}

TEST("element access") {
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};
  check_eq(xs["a"], 1);
  check_eq(xs["b"], 2);
  check_eq(xs["e"], 0);
}
