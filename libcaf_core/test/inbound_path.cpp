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

#define CAF_SUITE inbound_path
#include "caf/test/unit_test.hpp"

#include <string>

#include "caf/inbound_path.hpp"

using namespace std;
using namespace caf;

namespace {

#define PRINT(format, ...)                                                     \
  {                                                                            \
    char buf[200];                                                             \
    snprintf(buf, 200, format, __VA_ARGS__);                                   \
    CAF_MESSAGE(buf);                                                          \
  }

struct fixture {
  inbound_path::stats_t x;

  void calculate(int32_t total_items, int32_t total_time) {
    int32_t c = 1000;
    int32_t d = 100;
    int32_t n = total_items;
    int32_t t = total_time;
    int32_t m = t > 0 ? std::max((c * n) / t, 1) : 1;
    int32_t b = t > 0 ? std::max((d * n) / t, 1) : 1;
    PRINT("with a cycle C = %dns, desied complexity D = %d,", c, d);
    PRINT("number of items N = %d, and time delta t = %d:", n, t);
    PRINT("- throughput M = max(C * N / t, 1) = max(%d * %d / %d, 1) = %d",
          c, n, t, m);
    PRINT("- items/batch B = max(D * N / t, 1) = max(%d * %d / %d, 1) = %d",
          d, n, t, b);
    auto cr = x.calculate(timespan(c), timespan(d));
    CAF_CHECK_EQUAL(cr.items_per_batch, b);
    CAF_CHECK_EQUAL(cr.max_throughput, m);
  }

  void store(int32_t batch_size, int32_t calculation_time_ns) {
    inbound_path::stats_t::measurement m{batch_size,
                                         timespan{calculation_time_ns}};
    x.store(m);
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(inbound_path_tests, fixture)

CAF_TEST(default_constructed) {
  calculate(0, 0);
}

CAF_TEST(one_store) {
  CAF_MESSAGE("store a measurement for 500ns with batch size of 50");
  store(50, 500);
  calculate(50, 500);
}

CAF_TEST(multiple_stores) {
  CAF_MESSAGE("store measurements: (50, 500ns), (60, 400ns), (40, 600ns)");
  store(50, 500);
  store(40, 600);
  store(60, 400);
  calculate(150, 1500);
}

CAF_TEST_FIXTURE_SCOPE_END()
