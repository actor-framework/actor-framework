// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/fixture/flow.hpp"
#include "caf/test/test.hpp"

#include "caf/cow_string.hpp"
#include "caf/flow/multicaster.hpp"

#include <string>

using namespace caf;
using namespace std::literals;

WITH_FIXTURE(test::fixture::flow) {

TEST("combine_latest merges 2 observables") {
  auto src1 = caf::flow::multicaster<cow_string>(coordinator());
  auto src2 = caf::flow::multicaster<int>(coordinator());
  auto fn = [](const cow_string& x, int y) {
    auto result = x.str();
    result += std::to_string(y);
    return cow_string{result};
  };
  auto snk = make_auto_observer<cow_string>();
  auto cs = [](std::string str) { return cow_string{std::move(str)}; };
  auto uut = disposable{};
  SECTION("observable_builder") {
    uut = make_observable()
            .combine_latest(fn, src1.as_observable(), src2.as_observable())
            .subscribe(snk->as_observer());
  }
  SECTION("observable") {
    uut = src1.as_observable()
            .combine_latest(fn, src2.as_observable())
            .subscribe(snk->as_observer());
  }
  src1.push(cow_string{"A"s});
  run_flows();
  check(snk->buf.empty());
  src1.push(cow_string{"B"s});
  run_flows();
  check(snk->buf.empty());
  src2.push(7);
  run_flows();
  check_eq(snk->buf, std::vector{cs("B7")});
  src1.push(cow_string{"C"s});
  run_flows();
  check_eq(snk->buf, std::vector{cs("B7"), cs("C7")});
  src1.close();
  run_flows();
  check(snk->subscribed());
  src2.push(1);
  run_flows();
  check_eq(snk->buf, std::vector{cs("B7"), cs("C7"), cs("C1")});
  src2.close();
  run_flows();
  check(snk->completed());
}

TEST("combine_latest merges more than 2 observables") {
  auto src1 = caf::flow::multicaster<cow_string>(coordinator());
  auto src2 = caf::flow::multicaster<int>(coordinator());
  auto src3 = caf::flow::multicaster<int>(coordinator());
  auto fn = [](const cow_string& x, int y, int z) {
    auto result = x.str();
    result += std::to_string(y);
    result += std::to_string(z);
    return cow_string{result};
  };
  auto snk = make_auto_observer<cow_string>();
  auto cs = [](std::string str) { return cow_string{std::move(str)}; };
  auto uut = disposable{};
  SECTION("observable_builder") {
    uut = make_observable()
            .combine_latest(fn, src1.as_observable(), src2.as_observable(),
                            src3.as_observable())
            .subscribe(snk->as_observer());
  }
  SECTION("observable") {
    uut = src1.as_observable()
            .combine_latest(fn, src2.as_observable(), src3.as_observable())
            .subscribe(snk->as_observer());
  }
  src1.push(cow_string{"A"s});
  run_flows();
  check(snk->buf.empty());
  src1.push(cow_string{"B"s});
  run_flows();
  check(snk->buf.empty());
  src2.push(7);
  run_flows();
  src2.push(8);
  run_flows();
  src3.push(7);
  run_flows();
  check_eq(snk->buf, std::vector{cs("B87")});
  src1.push(cow_string{"C"s});
  run_flows();
  src2.push(1);
  run_flows();
  src3.push(2);
  run_flows();
  check_eq(snk->buf, std::vector{cs("B87"), cs("C87"), cs("C17"), cs("C12")});
  src1.close();
  run_flows();
  src2.push(3);
  run_flows();
  src3.push(4);
  run_flows();
  check_eq(snk->buf, std::vector{cs("B87"), cs("C87"), cs("C17"), cs("C12"),
                                 cs("C32"), cs("C34")});
  src2.close();
  run_flows();
  src3.close();
  run_flows();
  check(snk->completed());
}

TEST("combine_latest fails when one observable closes without emitting") {
  auto src1 = caf::flow::multicaster<cow_string>(coordinator());
  auto src2 = caf::flow::multicaster<int>(coordinator());
  auto fn = [](const cow_string& x, int y) {
    auto result = x.str();
    result += std::to_string(y);
    return cow_string{result};
  };
  auto snk = make_auto_observer<cow_string>();
  auto uut = disposable{};
  SECTION("observable_builder") {
    uut = make_observable()
            .combine_latest(fn, src1.as_observable(), src2.as_observable())
            .subscribe(snk->as_observer());
  }
  SECTION("observable") {
    uut = src1.as_observable()
            .combine_latest(fn, src2.as_observable())
            .subscribe(snk->as_observer());
  }
  src1.push(cow_string{"A"s});
  run_flows();
  check(snk->buf.empty());
  src1.push(cow_string{"B"s});
  run_flows();
  check(snk->buf.empty());
  run_flows();
  check(snk->subscribed());
  check(snk->buf.empty());
  src2.close();
  run_flows();
  check(!snk->subscribed());
  check(snk->aborted());
  src1.close();
  run_flows();
}

TEST("combine_latest fails when multiple observables close without emitting") {
  auto src1 = caf::flow::multicaster<cow_string>(coordinator());
  auto src2 = caf::flow::multicaster<int>(coordinator());
  auto fn = [](const cow_string& x, int y) {
    auto result = x.str();
    result += std::to_string(y);
    return cow_string{result};
  };
  auto snk = make_auto_observer<cow_string>();
  auto uut = disposable{};
  SECTION("observable_builder") {
    uut = make_observable()
            .combine_latest(fn, src1.as_observable(), src2.as_observable())
            .subscribe(snk->as_observer());
  }
  SECTION("observable") {
    uut = src1.as_observable()
            .combine_latest(fn, src2.as_observable())
            .subscribe(snk->as_observer());
  }
  check(snk->buf.empty());
  src1.close();
  run_flows();
  check(snk->buf.empty());
  check(snk->aborted());
  check(!snk->subscribed());
  src2.close();
  run_flows();
  check(snk->buf.empty());
  check(snk->aborted());
}

TEST("combine_latest fails when a source observable emits an error") {
  auto src1 = caf::flow::multicaster<cow_string>(coordinator());
  auto src2 = caf::flow::multicaster<int>(coordinator());
  auto fn = [](const cow_string& x, int y) {
    auto result = x.str();
    result += std::to_string(y);
    return cow_string{result};
  };
  auto snk = make_auto_observer<cow_string>();
  auto cs = [](std::string str) { return cow_string{std::move(str)}; };
  auto uut = disposable{};
  SECTION("observable_builder") {
    uut = make_observable()
            .combine_latest(fn, src1.as_observable(), src2.as_observable())
            .subscribe(snk->as_observer());
  }
  SECTION("observable") {
    uut = src1.as_observable()
            .combine_latest(fn, src2.as_observable())
            .subscribe(snk->as_observer());
  }
  src1.push(cow_string{"A"s});
  run_flows();
  check(snk->buf.empty());
  src1.push(cs("B"));
  run_flows();
  check(snk->buf.empty());
  src2.push(7);
  run_flows();
  check_eq(snk->buf, std::vector{cs("B7")});
  src1.push(cow_string{"C"s});
  run_flows();
  src2.push(1);
  run_flows();
  check_eq(snk->buf, std::vector{cs("B7"), cs("C7"), cs("C1")});
  src1.push(cs("D"));
  run_flows();
  src2.push(2);
  run_flows();
  src1.abort(make_error(sec::runtime_error));
  run_flows();
  check(snk->aborted());
  check(!snk->subscribed());
  check_eq(snk->err, make_error(sec::runtime_error));
}

} // WITH_FIXTURE(test::fixture::flow)
