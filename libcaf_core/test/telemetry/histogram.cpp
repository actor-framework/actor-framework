/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE telemetry.histogram

#include "caf/telemetry/histogram.hpp"

#include "caf/test/dsl.hpp"

#include <cmath>
#include <limits>

#include "caf/telemetry/gauge.hpp"

using namespace caf;
using namespace caf::telemetry;

CAF_TEST(double histograms use infinity for the last bucket) {
  dbl_histogram h1{.1, .2, .4, .8};
  CAF_CHECK_EQUAL(h1.buckets().size(), 5u);
  CAF_CHECK_EQUAL(h1.buckets().front().upper_bound, .1);
  CAF_CHECK(std::isinf(h1.buckets().back().upper_bound));
  CAF_CHECK_EQUAL(h1.sum(), 0.0);
}

CAF_TEST(integer histograms use int_max for the last bucket) {
  using limits = std::numeric_limits<int64_t>;
  int_histogram h1{1, 2, 4, 8};
  CAF_CHECK_EQUAL(h1.buckets().size(), 5u);
  CAF_CHECK_EQUAL(h1.buckets().front().upper_bound, 1);
  CAF_CHECK_EQUAL(h1.buckets().back().upper_bound, limits::max());
  CAF_CHECK_EQUAL(h1.sum(), 0);
}

CAF_TEST(histograms aggregate to buckets and keep a sum) {
  int_histogram h1{2, 4, 8};
  for (int64_t value = 1; value < 11; ++value)
    h1.observe(value);
  auto buckets = h1.buckets();
  CAF_REQUIRE_EQUAL(buckets.size(), 4u);
  CAF_CHECK_EQUAL(buckets[0].gauge.value(), 2); // 1, 2
  CAF_CHECK_EQUAL(buckets[1].gauge.value(), 2); // 3, 4
  CAF_CHECK_EQUAL(buckets[2].gauge.value(), 4); // 5, 6, 7, 8
  CAF_CHECK_EQUAL(buckets[3].gauge.value(), 2); // 9, 10
  CAF_CHECK_EQUAL(h1.sum(), 55);
}
