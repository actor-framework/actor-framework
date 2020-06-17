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

#define CAF_SUITE telemetry.timer

#include "caf/telemetry/timer.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;
using namespace caf::telemetry;

CAF_TEST(timers observe how much time passes in a scope) {
  dbl_histogram h1{1, 2, 4, 8};
  CAF_MESSAGE("timers call observe() with the measured time");
  {
    timer t{&h1};
    CAF_CHECK_EQUAL(t.histogram_ptr(), &h1);
    CAF_CHECK_GREATER(t.started().time_since_epoch().count(), 0);
  }
  CAF_CHECK_GREATER(h1.sum(), 0.0);
  CAF_MESSAGE("timers constructed with a nullptr have no effect");
  {
    timer t{nullptr};
    CAF_CHECK_EQUAL(t.histogram_ptr(), nullptr);
  }
}
