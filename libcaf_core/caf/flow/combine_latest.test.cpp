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

TEST("combine_latest merges multiple observables") {
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
  // TODO: add test for calling `combine_latest` on an observable
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

} // WITH_FIXTURE(test::fixture::flow)
