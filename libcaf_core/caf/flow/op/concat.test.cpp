// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/op/concat.hpp"

#include "caf/test/fixture/flow.hpp"
#include "caf/test/nil.hpp"
#include "caf/test/test.hpp"

#include "caf/flow/observable.hpp"

#include <vector>

using namespace caf;

using std::vector;

namespace {

template <class T, class... Args>
auto seq(const std::vector<T>& xs, Args&&... args) {
  auto res = xs;
  (res.insert(res.end(), args.begin(), args.end()), ...);
  return res;
}

WITH_FIXTURE(test::fixture::flow) {
TEST("concat operators with empty inputs emit no values") {
  auto nil = [this] { return make_observable().empty<int>().as_observable(); };
  SECTION("blueprint") {
    check_eq(collect(nil().concat(nil())), vector<int>{});
    check_eq(collect(just(nil()).concat()), vector<int>{});
  }
  SECTION("observable") {
    check_eq(collect(build(nil()).concat(nil())), vector<int>{});
    check_eq(collect(build(just(nil())).concat()), vector<int>{});
  }
}

TEST("concat operators with error inputs forward the error") {
  auto nil = [this] { return make_observable().empty<int>().as_observable(); };
  auto err = [this] { return make_observable().fail<int>(sec::runtime_error); };
  SECTION("blueprint") {
    check_eq(collect(nil().concat(err())), sec::runtime_error);
    check_eq(collect(err().concat(nil())), sec::runtime_error);
    check_eq(collect(just(err()).concat()), sec::runtime_error);
  }
  SECTION("observable") {
    check_eq(collect(nil().concat(err())), sec::runtime_error);
    check_eq(collect(err().concat(nil())), sec::runtime_error);
    check_eq(collect(just(err()).concat()), sec::runtime_error);
  }
}

TEST("concat operators with non-empty inputs emit all values") {
  auto r1 = [this] { return make_observable().iota(0).take(10); };
  auto r2 = [this] { return make_observable().iota(10).take(10); };
  auto res1 = vector{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  auto res2 = vector{10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
  SECTION("blueprint") {
    check_eq(collect(r1().concat(r2())), seq(res1, res2));
    check_eq(collect(r2().concat(r1())), seq(res2, res1));
    check_eq(collect(just(r1()).concat()), res1);
  }
  SECTION("observable") {
    check_eq(collect(build(r1()).concat(r2())), seq(res1, res2));
    check_eq(collect(build(r2()).concat(r1())), seq(res2, res1));
    check_eq(collect(build(just(r1())).concat()), res1);
  }
}

} // WITH_FIXTURE(test::fixture::flow)

} // namespace
