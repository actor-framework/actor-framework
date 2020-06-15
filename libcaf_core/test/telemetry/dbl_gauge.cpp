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

#define CAF_SUITE telemetry.dbl_gauge

#include "caf/telemetry/dbl_gauge.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

CAF_TEST(double gauges can increment and decrement) {
  telemetry::dbl_gauge g;
  CAF_MESSAGE("gauges start at 0");
  CAF_CHECK_EQUAL(g.value(), 0.0);
  CAF_MESSAGE("gauges are incrementable");
  g.inc();
  g.inc(2.0);
  CAF_CHECK_EQUAL(g.value(), 3.0);
  CAF_MESSAGE("gauges are decrementable");
  g.dec();
  g.dec(5.0);
  CAF_CHECK_EQUAL(g.value(), -3.0);
  CAF_MESSAGE("gauges allow setting values");
  g.value(42.0);
  CAF_CHECK_EQUAL(g.value(), 42.0);
  CAF_MESSAGE("users can create gauges with custom start values");
  CAF_CHECK_EQUAL(telemetry::dbl_gauge{42.0}.value(), 42.0);
}
