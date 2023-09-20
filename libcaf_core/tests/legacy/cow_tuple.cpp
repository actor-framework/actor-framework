// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE cow_tuple

#include "caf/cow_tuple.hpp"

#include "core-test.hpp"

using std::make_tuple;
using std::string;
using std::tuple;

using namespace caf;

CAF_TEST(default_construction) {
  cow_tuple<string, string> x;
  CHECK_EQ(x.unique(), true);
  CHECK_EQ(get<0>(x), "");
  CHECK_EQ(get<1>(x), "");
}

CAF_TEST(value_construction) {
  cow_tuple<int, int> x{1, 2};
  CHECK_EQ(x.unique(), true);
  CHECK_EQ(get<0>(x), 1);
  CHECK_EQ(get<1>(x), 2);
  CHECK_EQ(x, make_cow_tuple(1, 2));
}

CAF_TEST(copy_construction) {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{x};
  CHECK_EQ(x, y);
  CHECK_EQ(x.ptr(), y.ptr());
  CHECK_EQ(x.unique(), false);
  CHECK_EQ(y.unique(), false);
}

CAF_TEST(move_construction) {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{std::move(x)};
  CHECK_EQ(x.ptr(), nullptr); // NOLINT(bugprone-use-after-move)
  CHECK_EQ(y, make_tuple(1, 2));
  CHECK_EQ(y.unique(), true);
}

CAF_TEST(copy_assignment) {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{3, 4};
  CHECK_NE(x, y);
  x = y;
  CHECK_EQ(x, y);
  CHECK_EQ(x.ptr(), y.ptr());
  CHECK_EQ(x.unique(), false);
  CHECK_EQ(y.unique(), false);
}

CAF_TEST(move_assignment) {
  cow_tuple<int, int> x{1, 2};
  cow_tuple<int, int> y{3, 4};
  CHECK_NE(x, y);
  x = std::move(y);
  CHECK_EQ(x, make_tuple(3, 4));
  CHECK_EQ(x.unique(), true);
}

CAF_TEST(make_cow_tuple) {
  cow_tuple<int, int> x{1, 2};
  auto y = make_cow_tuple(1, 2);
  CHECK_EQ(x, y);
  CHECK_EQ(x.unique(), true);
  CHECK_EQ(y.unique(), true);
}

CAF_TEST(unsharing) {
  auto x = make_cow_tuple(string{"old"}, string{"school"});
  auto y = x;
  CHECK_EQ(x.unique(), false);
  CHECK_EQ(y.unique(), false);
  get<0>(y.unshared()) = "new";
  CHECK_EQ(x.unique(), true);
  CHECK_EQ(y.unique(), true);
  CHECK_EQ(x.data(), make_tuple("old", "school"));
  CHECK_EQ(y.data(), make_tuple("new", "school"));
}

CAF_TEST(to_string) {
  auto x = make_cow_tuple(1, string{"abc"});
  CHECK_EQ(deep_to_string(x), R"__([1, "abc"])__");
}

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

CAF_TEST(serialization) {
  auto x = make_cow_tuple(1, 2, 3);
  auto y = roundtrip(x);
  CHECK_EQ(x, y);
  CHECK_EQ(x.unique(), true);
  CHECK_EQ(y.unique(), true);
  CHECK_NE(x.ptr(), y.ptr());
}

END_FIXTURE_SCOPE()
