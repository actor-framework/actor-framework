// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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

#define CAF_SUITE detail.tick_emitter

#include "caf/detail/tick_emitter.hpp"

#include "core-test.hpp"

#include <vector>

#include "caf/detail/gcd.hpp"
#include "caf/timestamp.hpp"

using std::vector;

using namespace caf;

using time_point = caf::detail::tick_emitter::time_point;

namespace {

timespan credit_interval{200};
timespan force_batch_interval{50};

} // namespace

CAF_TEST(start_and_stop) {
  detail::tick_emitter x;
  detail::tick_emitter y{time_point{timespan{100}}};
  detail::tick_emitter z;
  z.start(time_point{timespan{100}});
  CHECK_EQ(x.started(), false);
  CHECK_EQ(y.started(), true);
  CHECK_EQ(z.started(), true);
  for (auto t : {&x, &y, &z})
    t->stop();
  CHECK_EQ(x.started(), false);
  CHECK_EQ(y.started(), false);
  CHECK_EQ(z.started(), false);
}

CAF_TEST(ticks) {
  auto cycle = detail::gcd(credit_interval.count(),
                           force_batch_interval.count());
  CHECK_EQ(cycle, 50);
  auto force_batch_frequency
    = static_cast<size_t>(force_batch_interval.count() / cycle);
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
  MESSAGE("trigger 4 ticks");
  tctrl.update(time_point{timespan{300}}, f);
  CHECK_EQ(deep_to_string(ticks), "[1, 2, 3, 4]");
  CHECK_EQ(force_batch_triggers, 4lu);
  CHECK_EQ(credit_triggers, 1lu);
  MESSAGE("trigger 3 more ticks");
  tctrl.update(time_point{timespan{475}}, f);
  CHECK_EQ(deep_to_string(ticks), "[1, 2, 3, 4, 5, 6, 7]");
  CHECK_EQ(force_batch_triggers, 7lu);
  CHECK_EQ(credit_triggers, 1lu);
}

CAF_TEST(timeouts) {
  timespan interval{50};
  time_point start{timespan{100}};
  auto now = start;
  detail::tick_emitter tctrl{now};
  tctrl.interval(interval);
  MESSAGE("advance until the first 5-tick-period ends");
  now += interval * 5;
  auto bitmask = tctrl.timeouts(now, {5, 7});
  CHECK_EQ(bitmask, 0x01u);
  MESSAGE("advance until the first 7-tick-period ends");
  now += interval * 2;
  bitmask = tctrl.timeouts(now, {5, 7});
  CHECK_EQ(bitmask, 0x02u);
  MESSAGE("advance until both tick period ends");
  now += interval * 7;
  bitmask = tctrl.timeouts(now, {5, 7});
  CHECK_EQ(bitmask, 0x03u);
  MESSAGE("advance until both tick period end multiple times");
  now += interval * 21;
  bitmask = tctrl.timeouts(now, {5, 7});
  CHECK_EQ(bitmask, 0x03u);
  MESSAGE("advance without any timeout");
  now += interval * 1;
  bitmask = tctrl.timeouts(now, {5, 7});
  CHECK_EQ(bitmask, 0x00u);
}

CAF_TEST(next_timeout) {
  timespan interval{50};
  time_point start{timespan{100}};
  auto now = start;
  detail::tick_emitter tctrl{now};
  tctrl.interval(interval);
  MESSAGE("advance until the first 5-tick-period ends");
  auto next = tctrl.next_timeout(now, {5, 7});
  CHECK_EQ(next, start + timespan(5 * interval));
  MESSAGE("advance until the first 7-tick-period ends");
  now = start + timespan(5 * interval);
  next = tctrl.next_timeout(now, {5, 7});
  CHECK_EQ(next, start + timespan(7 * interval));
  MESSAGE("advance until the second 5-tick-period ends");
  now = start + timespan(7 * interval);
  next = tctrl.next_timeout(now, {5, 7});
  CHECK_EQ(next, start + timespan((2 * 5) * interval));
  MESSAGE("advance until the second 7-tick-period ends");
  now = start + timespan(11 * interval);
  next = tctrl.next_timeout(now, {5, 7});
  CHECK_EQ(next, start + timespan((2 * 7) * interval));
}
