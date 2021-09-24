// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE dictionary

#include "caf/dictionary.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

using int_dict = dictionary<int>;

} // namespace

CAF_TEST(construction and comparison) {
  int_dict xs;
  CHECK_EQ(xs.empty(), true);
  CHECK_EQ(xs.size(), 0u);
  int_dict ys{{"foo", 1}, {"bar", 2}};
  CHECK_EQ(ys.empty(), false);
  CHECK_EQ(ys.size(), 2u);
  CHECK_NE(xs, ys);
  int_dict zs{ys.begin(), ys.end()};
  CHECK_EQ(zs.empty(), false);
  CHECK_EQ(zs.size(), 2u);
  CHECK_EQ(ys, zs);
  zs.clear();
  CHECK_EQ(zs.empty(), true);
  CHECK_EQ(zs.size(), 0u);
  CHECK_EQ(xs, zs);
}

CAF_TEST(iterators) {
  using std::equal;
  using vector_type = std::vector<int_dict::value_type>;
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}};
  vector_type ys{{"a", 1}, {"b", 2}, {"c", 3}};
  CHECK(equal(xs.begin(), xs.end(), ys.begin()));
  CHECK(equal(xs.cbegin(), xs.cend(), ys.cbegin()));
  CHECK(equal(xs.rbegin(), xs.rend(), ys.rbegin()));
  CHECK(equal(xs.crbegin(), xs.crend(), ys.crbegin()));
}

CAF_TEST(swapping) {
  int_dict xs{{"foo", 1}, {"bar", 2}};
  int_dict ys;
  int_dict zs{{"foo", 1}, {"bar", 2}};
  CHECK_NE(xs, ys);
  CHECK_NE(ys, zs);
  CHECK_EQ(xs, zs);
  xs.swap(ys);
  CHECK_NE(xs, ys);
  CHECK_EQ(ys, zs);
  CHECK_NE(xs, zs);
}

CAF_TEST(emplacing) {
  int_dict xs;
  CHECK_EQ(xs.emplace("x", 1).second, true);
  CHECK_EQ(xs.emplace("y", 2).second, true);
  CHECK_EQ(xs.emplace("y", 3).second, false);
}

CAF_TEST(insertion) {
  int_dict xs;
  CHECK_EQ(xs.insert("a", 1).second, true);
  CHECK_EQ(xs.insert("b", 2).second, true);
  CHECK_EQ(xs.insert("c", 3).second, true);
  CHECK_EQ(xs.insert("c", 4).second, false);
  int_dict ys;
  CHECK_EQ(ys.insert_or_assign("a", 1).second, true);
  CHECK_EQ(ys.insert_or_assign("b", 2).second, true);
  CHECK_EQ(ys.insert_or_assign("c", 0).second, true);
  CHECK_EQ(ys.insert_or_assign("c", 3).second, false);
  CHECK_EQ(xs, ys);
}

CAF_TEST(insertion with hint) {
  int_dict xs;
  auto xs_last = xs.end();
  auto xs_insert = [&](string_view key, int val) {
    xs_last = xs.insert(xs_last, key, val);
  };
  xs_insert("a", 1);
  xs_insert("c", 3);
  xs_insert("b", 2);
  xs_insert("c", 4);
  int_dict ys;
  auto ys_last = ys.end();
  auto ys_insert_or_assign = [&](string_view key, int val) {
    ys_last = ys.insert_or_assign(ys_last, key, val);
  };
  ys_insert_or_assign("a", 1);
  ys_insert_or_assign("c", 0);
  ys_insert_or_assign("b", 2);
  ys_insert_or_assign("c", 3);
  CHECK_EQ(xs, ys);
}

CAF_TEST(bounds) {
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};
  const int_dict& const_xs = xs;
  CHECK_EQ(xs.lower_bound("c")->first, "c");
  CHECK_EQ(xs.upper_bound("c")->first, "d");
  CHECK_EQ(const_xs.lower_bound("c")->first, "c");
  CHECK_EQ(const_xs.upper_bound("c")->first, "d");
}

CAF_TEST(find) {
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};
  const int_dict& const_xs = xs;
  CHECK_EQ(xs.find("e"), xs.end());
  CHECK_EQ(xs.find("a")->second, 1);
  CHECK_EQ(xs.find("c")->second, 3);
  CHECK_EQ(const_xs.find("e"), xs.end());
  CHECK_EQ(const_xs.find("a")->second, 1);
  CHECK_EQ(const_xs.find("c")->second, 3);
}

CAF_TEST(element access) {
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};
  CHECK_EQ(xs["a"], 1);
  CHECK_EQ(xs["b"], 2);
  CHECK_EQ(xs["e"], 0);
}
