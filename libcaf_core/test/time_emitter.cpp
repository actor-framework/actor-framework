/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

// This test simulates a complex multiplexing over multiple layers of WDRR
// scheduled queues. The goal is to reduce the complex mailbox management of
// CAF to its bare bones in order to test whether the multiplexing of stream
// traffic and asynchronous messages works as intended.
//
// The setup is a fixed WDRR queue with three nestes queues. The first nested
// queue stores asynchronous messages, the second one upstream messages, and
// the last queue is a dynamic WDRR queue storing downstream messages.
//
// We mock just enough of an actor to use the streaming classes and put them to
// work in a pipeline with 2 or 3 stages.

#define CAF_SUITE time_emitter

#include <vector>

#include "caf/detail/gcd.hpp"
#include "caf/detail/tick_emitter.hpp"

#include "caf/test/unit_test.hpp"

using std::vector;

using namespace caf;

CAF_TEST(ticks) {
  using timespan = std::chrono::microseconds;
  timespan credit_interval{200};
  timespan force_batch_interval{50};
  auto cycle = detail::gcd(credit_interval.count(),
                           force_batch_interval.count());
  CAF_CHECK_EQUAL(cycle, 50);
  auto force_batch_frequency = force_batch_interval.count() / cycle;
  auto credit_frequency = credit_interval.count() / cycle;
  using time_point = std::chrono::steady_clock::time_point;
  detail::tick_emitter tctrl{time_point{timespan{100}}};
  tctrl.interval(timespan{cycle});
  vector<long> ticks;
  int force_batch_triggers = 0;
  int credit_triggers = 0;
  auto f = [&](long tick_id) {
    ticks.push_back(tick_id);
    if (tick_id % force_batch_frequency == 0)
      ++force_batch_triggers;
    if (tick_id % credit_frequency == 0)
      ++credit_triggers;
  };
  CAF_MESSAGE("trigger 4 ticks");
  tctrl.update(time_point{timespan{300}}, f);
  CAF_CHECK_EQUAL(deep_to_string(ticks), "[1, 2, 3, 4]");
  CAF_CHECK_EQUAL(force_batch_triggers, 4);
  CAF_CHECK_EQUAL(credit_triggers, 1);
  CAF_MESSAGE("trigger 3 more ticks");
  tctrl.update(time_point{timespan{475}}, f);
  CAF_CHECK_EQUAL(deep_to_string(ticks), "[1, 2, 3, 4, 5, 6, 7]");
  CAF_CHECK_EQUAL(force_batch_triggers, 7);
  CAF_CHECK_EQUAL(credit_triggers, 1);
}

