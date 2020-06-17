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

#define CAF_SUITE telemetry.counter

#include "caf/telemetry/counter.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

CAF_TEST(double counters can only increment) {
  telemetry::dbl_counter g;
  CAF_MESSAGE("counters start at 0");
  CAF_CHECK_EQUAL(g.value(), 0.0);
  CAF_MESSAGE("counters are incrementable");
  g.inc();
  g.inc(2.0);
  CAF_CHECK_EQUAL(g.value(), 3.0);
  CAF_MESSAGE("users can create counters with custom start values");
  CAF_CHECK_EQUAL(telemetry::dbl_counter{42.0}.value(), 42.0);
}

CAF_TEST(integer counters can only increment) {
  telemetry::int_counter g;
  CAF_MESSAGE("counters start at 0");
  CAF_CHECK_EQUAL(g.value(), 0);
  CAF_MESSAGE("counters are incrementable");
  g.inc();
  g.inc(2);
  CAF_CHECK_EQUAL(g.value(), 3);
  CAF_MESSAGE("users can create counters with custom start values");
  CAF_CHECK_EQUAL(telemetry::int_counter{42}.value(), 42);
}
