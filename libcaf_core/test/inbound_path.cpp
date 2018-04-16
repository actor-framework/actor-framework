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

template <class... Ts>
void print(const char* format, Ts... xs) {
  char buf[200];
  snprintf(buf, 200, format, xs...);
  CAF_MESSAGE(buf);
}

struct fixture {
  inbound_path::stats_t x;
  size_t sampling_size = inbound_path::stats_sampling_size;

  fixture() {
    CAF_CHECK_EQUAL(x.measurements.size(), sampling_size);
    CAF_CHECK_EQUAL(sampling_size % 2, 0u);
  }

  void calculate(int32_t total_items, int32_t total_time) {
    int32_t c = 1000;
    int32_t d = 100;
    int32_t n = total_items;
    int32_t t = total_time;
    int32_t m = t > 0 ? std::max((c * n) / t, 1) : 1;
    int32_t b = t > 0 ? std::max((d * n) / t, 1) : 1;
    print("with a cycle C = %ldns, desied complexity D = %ld,", c, d);
    print("number of items N = %ld, and time delta t = %ld:", n, t);
    print("- throughput M = max(C * N / t, 1) = max(%ld * %ld / %ld, 1) = %ld",
          c, n, t, m);
    print("- items/batch B = max(D * N / t, 1) = max(%ld * %ld / %ld, 1) = %ld",
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
  CAF_MESSAGE("store a measurement: (50, 500ns), (60, 400ns), (40, 600ns)");
  store(50, 500);
  store(40, 600);
  store(60, 400);
  calculate(150, 1500);
}

CAF_TEST(overriding_stores) {
  CAF_MESSAGE("fill measurements with (100, 1000ns)");
  for (size_t i = 0; i < sampling_size; ++i)
    store(100, 1000);
  calculate(100, 1000);
  CAF_MESSAGE("override first half of the measurements with (10, 1000ns)");
  for (size_t i = 0; i < sampling_size / 2; ++i)
    store(10, 1000);
  calculate(55, 1000);
  CAF_MESSAGE("override second half of the measurements with (10, 1000ns)");
  for (size_t i = 0; i < sampling_size / 2; ++i)
    store(10, 1000);
  calculate(10, 1000);
}

CAF_TEST_FIXTURE_SCOPE_END()
