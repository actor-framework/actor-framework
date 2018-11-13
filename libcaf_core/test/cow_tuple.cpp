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

#define CAF_SUITE cow_tuple

#include "caf/cow_tuple.hpp"

#include "caf/test/dsl.hpp"

using std::make_tuple;
using std::string;
using std::tuple;

using namespace caf;

CAF_TEST(default_construction) {
  cow_tuple<string, string> x;
  CAF_CHECK_EQUAL(x.unique(), true);
  CAF_CHECK_EQUAL(get<0>(x), "");
  CAF_CHECK_EQUAL(get<1>(x), "");
}

CAF_TEST(value_construction) {
  cow_tuple<int, int> x{1, 2};
  CAF_CHECK_EQUAL(x.unique(), true);
  CAF_CHECK_EQUAL(get<0>(x), 1);
  CAF_CHECK_EQUAL(get<1>(x), 2);
  CAF_CHECK_EQUAL(x, make_cow_tuple(1, 2));
}

CAF_TEST(copy_construction) {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{x};
  CAF_CHECK_EQUAL(x, y);
  CAF_CHECK_EQUAL(x.ptr(), y.ptr());
  CAF_CHECK_EQUAL(x.unique(), false);
  CAF_CHECK_EQUAL(y.unique(), false);
}

CAF_TEST(move_construction) {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{std::move(x)};
  CAF_CHECK_EQUAL(x.ptr(), nullptr);
  CAF_CHECK_EQUAL(y, make_tuple(1, 2));
  CAF_CHECK_EQUAL(y.unique(), true);
}

CAF_TEST(copy_assignment) {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{3, 4};
  CAF_CHECK_NOT_EQUAL(x, y);
  x = y;
  CAF_CHECK_EQUAL(x, y);
  CAF_CHECK_EQUAL(x.ptr(), y.ptr());
  CAF_CHECK_EQUAL(x.unique(), false);
  CAF_CHECK_EQUAL(y.unique(), false);
}

CAF_TEST(move_assignment) {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{3, 4};
  CAF_CHECK_NOT_EQUAL(x, y);
  x = std::move(y);
  CAF_CHECK_EQUAL(x, make_tuple(3, 4));
  CAF_CHECK_EQUAL(x.unique(), true);
  CAF_CHECK_EQUAL(y.ptr(), nullptr);
}

CAF_TEST(make_cow_tuple) {
  cow_tuple<int, int> x{1, 2};
  auto y = make_cow_tuple(1, 2);
  CAF_CHECK_EQUAL(x, y);
  CAF_CHECK_EQUAL(x.unique(), true);
  CAF_CHECK_EQUAL(y.unique(), true);
}

CAF_TEST(unsharing) {
  auto x = make_cow_tuple(string{"old"}, string{"school"});
  auto y = x;
  CAF_CHECK_EQUAL(x.unique(), false);
  CAF_CHECK_EQUAL(y.unique(), false);
  get<0>(y.unshared()) = "new";
  CAF_CHECK_EQUAL(x.unique(), true);
  CAF_CHECK_EQUAL(y.unique(), true);
  CAF_CHECK_EQUAL(x.data(), make_tuple("old", "school"));
  CAF_CHECK_EQUAL(y.data(), make_tuple("new", "school"));
}

CAF_TEST(to_string) {
  auto x = make_cow_tuple(1, string{"abc"});
  CAF_CHECK_EQUAL(deep_to_string(x), "(1, \"abc\")");
}

CAF_TEST_FIXTURE_SCOPE(cow_tuple_tests, test_coordinator_fixture<>)

CAF_TEST(serialization) {
  auto x = make_cow_tuple(1, 2, 3);
  auto y = roundtrip(x);
  CAF_CHECK_EQUAL(x, y);
  CAF_CHECK_EQUAL(x.unique(), true);
  CAF_CHECK_EQUAL(y.unique(), true);
  CAF_CHECK_NOT_EQUAL(x.ptr(), y.ptr());
}

CAF_TEST_FIXTURE_SCOPE_END()
