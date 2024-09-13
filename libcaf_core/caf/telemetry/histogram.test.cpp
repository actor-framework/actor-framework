// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/telemetry/histogram.hpp"

#include "caf/test/approx.hpp"
#include "caf/test/test.hpp"

#include "caf/telemetry/gauge.hpp"

#include <cmath>
#include <limits>

using namespace caf;
using namespace caf::telemetry;

TEST("double histograms use infinity for the last bucket") {
  dbl_histogram h1{.1, .2, .4, .8};
  check_eq(h1.buckets().size(), 5u);
  check_eq(h1.buckets().front().upper_bound, test::approx{.1});
  check(std::isinf(h1.buckets().back().upper_bound));
  check_eq(h1.sum(), test::approx{0.0});
}

TEST("integer histograms use int_max for the last bucket") {
  using limits = std::numeric_limits<int64_t>;
  int_histogram h1{1, 2, 4, 8};
  check_eq(h1.buckets().size(), 5u);
  check_eq(h1.buckets().front().upper_bound, 1);
  check_eq(h1.buckets().back().upper_bound, limits::max());
  check_eq(h1.sum(), 0);
}

TEST("histograms aggregate to buckets and keep a sum") {
  int_histogram h1{2, 4, 8};
  for (int64_t value = 1; value < 11; ++value)
    h1.observe(value);
  auto buckets = h1.buckets();
  require_eq(buckets.size(), 4u);
  check_eq(buckets[0].count.value(), 2); // 1, 2
  check_eq(buckets[1].count.value(), 2); // 3, 4
  check_eq(buckets[2].count.value(), 4); // 5, 6, 7, 8
  check_eq(buckets[3].count.value(), 2); // 9, 10
  check_eq(h1.sum(), 55);
}
