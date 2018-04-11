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

#define CAF_SUITE tick_emitter

#include <vector>

#include "caf/timestamp.hpp"

#include "caf/detail/gcd.hpp"
#include "caf/detail/tick_emitter.hpp"

#include "caf/test/unit_test.hpp"

using std::vector;

using namespace caf;

using time_point = caf::detail::tick_emitter::time_point;

namespace {

timespan credit_interval{200};
timespan force_batch_interval{50};

} // namespace <anonymous>

CAF_TEST(start_and_stop) {
  detail::tick_emitter x;
  detail::tick_emitter y{time_point{timespan{100}}};
  detail::tick_emitter z;
  z.start(time_point{timespan{100}});
  CAF_CHECK_EQUAL(x.started(), false);
  CAF_CHECK_EQUAL(y.started(), true);
  CAF_CHECK_EQUAL(z.started(), true);
  for (auto t : {&x, &y, &z})
    t->stop();
  CAF_CHECK_EQUAL(x.started(), false);
  CAF_CHECK_EQUAL(y.started(), false);
  CAF_CHECK_EQUAL(z.started(), false);
}

CAF_TEST(ticks) {
  auto cycle = detail::gcd(credit_interval.count(),
                           force_batch_interval.count());
  CAF_CHECK_EQUAL(cycle, 50);
  auto force_batch_frequency =
    static_cast<size_t>(force_batch_interval.count() / cycle);
  auto credit_frequency = static_cast<size_t>(credit_interval.count() / cycle);
  detail::tick_emitter tctrl{time_point{timespan{100}}};
  tctrl.interval(timespan{cycle});
  vector<size_t> ticks;
  size_t force_batch_triggers = 0;
  size_t credit_triggers = 0;
  auto f = [&](size_t tick_id) {
    ticks.push_back(tick_id);
    if (tick_id % force_batch_frequency == 0)
      ++force_batch_triggers;
    if (tick_id % credit_frequency == 0)
      ++credit_triggers;
  };
  CAF_MESSAGE("trigger 4 ticks");
  tctrl.update(time_point{timespan{300}}, f);
  CAF_CHECK_EQUAL(deep_to_string(ticks), "[1, 2, 3, 4]");
  CAF_CHECK_EQUAL(force_batch_triggers, 4lu);
  CAF_CHECK_EQUAL(credit_triggers, 1lu);
  CAF_MESSAGE("trigger 3 more ticks");
  tctrl.update(time_point{timespan{475}}, f);
  CAF_CHECK_EQUAL(deep_to_string(ticks), "[1, 2, 3, 4, 5, 6, 7]");
  CAF_CHECK_EQUAL(force_batch_triggers, 7lu);
  CAF_CHECK_EQUAL(credit_triggers, 1lu);
}

CAF_TEST(timeouts) {
  timespan interval{50};
  time_point start{timespan{100}};
  auto now = start;
  detail::tick_emitter tctrl{now};
  tctrl.interval(interval);
  CAF_MESSAGE("advance until the first 5-tick-period ends");
  now += interval * 5;
  auto bitmask = tctrl.timeouts(now, {5, 7});
  CAF_CHECK_EQUAL(bitmask, 0x01u);
  CAF_MESSAGE("advance until the first 7-tick-period ends");
  now += interval * 2;
  bitmask = tctrl.timeouts(now, {5, 7});
  CAF_CHECK_EQUAL(bitmask, 0x02u);
  CAF_MESSAGE("advance until both tick period ends");
  now += interval * 7;
  bitmask = tctrl.timeouts(now, {5, 7});
  CAF_CHECK_EQUAL(bitmask, 0x03u);
  CAF_MESSAGE("advance until both tick period end multiple times");
  now += interval * 21;
  bitmask = tctrl.timeouts(now, {5, 7});
  CAF_CHECK_EQUAL(bitmask, 0x03u);
  CAF_MESSAGE("advance without any timeout");
  now += interval * 1;
  bitmask = tctrl.timeouts(now, {5, 7});
  CAF_CHECK_EQUAL(bitmask, 0x00u);
}

CAF_TEST(next_timeout) {
  timespan interval{50};
  time_point start{timespan{100}};
  auto now = start;
  detail::tick_emitter tctrl{now};
  tctrl.interval(interval);
  CAF_MESSAGE("advance until the first 5-tick-period ends");
  auto next = tctrl.next_timeout(now, {5, 7});
  CAF_CHECK_EQUAL(next, start + timespan(5 * interval));
  CAF_MESSAGE("advance until the first 7-tick-period ends");
  now = start + timespan(5 * interval);
  next = tctrl.next_timeout(now, {5, 7});
  CAF_CHECK_EQUAL(next, start + timespan(7 * interval));
  CAF_MESSAGE("advance until the second 5-tick-period ends");
  now = start + timespan(7 * interval);
  next = tctrl.next_timeout(now, {5, 7});
  CAF_CHECK_EQUAL(next, start + timespan((2 * 5) * interval));
  CAF_MESSAGE("advance until the second 7-tick-period ends");
  now = start + timespan(11 * interval);
  next = tctrl.next_timeout(now, {5, 7});
  CAF_CHECK_EQUAL(next, start + timespan((2 * 7) * interval));
}

