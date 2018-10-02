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

#define CAF_SUITE dictionary
#include "caf/test/dsl.hpp"

#include "caf/dictionary.hpp"

using namespace caf;

namespace {

using int_dict = dictionary<int>;

struct fixture {

};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(dictionary_tests, fixture)

CAF_TEST(construction and comparions) {
  int_dict xs;
  CAF_CHECK_EQUAL(xs.empty(), true);
  CAF_CHECK_EQUAL(xs.size(), 0u);
  int_dict ys{{"foo", 1}, {"bar", 2}};
  CAF_CHECK_EQUAL(ys.empty(), false);
  CAF_CHECK_EQUAL(ys.size(), 2u);
  CAF_CHECK_NOT_EQUAL(xs, ys);
  int_dict zs{ys.begin(), ys.end()};
  CAF_CHECK_EQUAL(zs.empty(), false);
  CAF_CHECK_EQUAL(zs.size(), 2u);
  CAF_CHECK_EQUAL(ys, zs);
  zs.clear();
  CAF_CHECK_EQUAL(zs.empty(), true);
  CAF_CHECK_EQUAL(zs.size(), 0u);
  CAF_CHECK_EQUAL(xs, zs);
}

CAF_TEST(iterators) {
  using std::equal;
  using vector_type = std::vector<int_dict::value_type>;
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}};
  vector_type ys{{"a", 1}, {"b", 2}, {"c", 3}};
  CAF_CHECK(equal(xs.begin(), xs.end(), ys.begin()));
  CAF_CHECK(equal(xs.cbegin(), xs.cend(), ys.cbegin()));
  CAF_CHECK(equal(xs.rbegin(), xs.rend(), ys.rbegin()));
  CAF_CHECK(equal(xs.crbegin(), xs.crend(), ys.crbegin()));
}

CAF_TEST(swapping) {
  int_dict xs{{"foo", 1}, {"bar", 2}};
  int_dict ys;
  int_dict zs{{"foo", 1}, {"bar", 2}};
  CAF_CHECK_NOT_EQUAL(xs, ys);
  CAF_CHECK_NOT_EQUAL(ys, zs);
  CAF_CHECK_EQUAL(xs, zs);
  xs.swap(ys);
  CAF_CHECK_NOT_EQUAL(xs, ys);
  CAF_CHECK_EQUAL(ys, zs);
  CAF_CHECK_NOT_EQUAL(xs, zs);
}

CAF_TEST(emplacing) {
  int_dict xs;
  CAF_CHECK_EQUAL(xs.emplace("x", 1).second, true);
  CAF_CHECK_EQUAL(xs.emplace("y", 2).second, true);
  CAF_CHECK_EQUAL(xs.emplace("y", 3).second, false);
}

CAF_TEST(insertion) {
  int_dict xs;
  CAF_CHECK_EQUAL(xs.insert("a", 1).second, true);
  CAF_CHECK_EQUAL(xs.insert("b", 2).second, true);
  CAF_CHECK_EQUAL(xs.insert("c", 3).second, true);
  CAF_CHECK_EQUAL(xs.insert("c", 4).second, false);
  int_dict ys;
  CAF_CHECK_EQUAL(ys.insert_or_assign("a", 1).second, true);
  CAF_CHECK_EQUAL(ys.insert_or_assign("b", 2).second, true);
  CAF_CHECK_EQUAL(ys.insert_or_assign("c", 0).second, true);
  CAF_CHECK_EQUAL(ys.insert_or_assign("c", 3).second, false);
  CAF_CHECK_EQUAL(xs, ys);
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
  CAF_CHECK_EQUAL(xs, ys);
}

CAF_TEST(bounds) {
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};
  const int_dict& const_xs = xs;
  CAF_CHECK_EQUAL(xs.lower_bound("c")->first, "c");
  CAF_CHECK_EQUAL(xs.upper_bound("c")->first, "d");
  CAF_CHECK_EQUAL(const_xs.lower_bound("c")->first, "c");
  CAF_CHECK_EQUAL(const_xs.upper_bound("c")->first, "d");
}

CAF_TEST(find) {
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};
  const int_dict& const_xs = xs;
  CAF_CHECK_EQUAL(xs.find("e"), xs.end());
  CAF_CHECK_EQUAL(xs.find("a")->second, 1);
  CAF_CHECK_EQUAL(xs.find("c")->second, 3);
  CAF_CHECK_EQUAL(const_xs.find("e"), xs.end());
  CAF_CHECK_EQUAL(const_xs.find("a")->second, 1);
  CAF_CHECK_EQUAL(const_xs.find("c")->second, 3);
}

CAF_TEST(element access) {
  int_dict xs{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};
  CAF_CHECK_EQUAL(xs["a"], 1);
  CAF_CHECK_EQUAL(xs["b"], 2);
  CAF_CHECK_EQUAL(xs["e"], 0);
}

CAF_TEST_FIXTURE_SCOPE_END()
