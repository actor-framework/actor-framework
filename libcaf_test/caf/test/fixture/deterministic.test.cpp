// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/fixture/deterministic.hpp"

#include "caf/test/registry.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/anon_mail.hpp"
#include "caf/chrono.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/send.hpp"

#include <chrono>

using caf::anon_mail;

using namespace std::literals;

namespace {

struct my_int {
  int value;
};

template <class Inspector>
auto inspect(Inspector& f, my_int& x) {
  return f.object(x).fields(f.field("value", x.value));
}

bool operator==(my_int lhs, my_int rhs) noexcept {
  return lhs.value == rhs.value;
}

bool operator==(my_int lhs, int rhs) noexcept {
  return lhs.value == rhs;
}

} // namespace

CAF_BEGIN_TYPE_ID_BLOCK(deterministic_fixture_test, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(deterministic_fixture_test, (my_int))

CAF_END_TYPE_ID_BLOCK(deterministic_fixture_test)

WITH_FIXTURE(caf::test::fixture::deterministic) {

TEST("the deterministic fixture provides a deterministic scheduler") {
  auto initialized = std::make_shared<bool>(false);
  auto count = std::make_shared<int32_t>(0);
  auto worker = sys.spawn([initialized, count] {
    *initialized = true;
    return caf::behavior{
      [count](int32_t value) { *count += value; },
      [count](const std::string& str) {
        if (auto ival = caf::get_as<int32_t>(caf::config_value{str}))
          *count += *ival;
      },
    };
  });
  caf::scoped_actor self{sys};
  check(*initialized);
  check_eq(mail_count(worker), 0u);
  anon_mail(1).send(worker);
  check_eq(mail_count(worker), 1u);
  self->mail(2).send(worker);
  check_eq(mail_count(worker), 2u);
  anon_mail(3).send(worker);
  check_eq(mail_count(worker), 3u);
  SECTION("calling dispatch_message processes a single message") {
    check(dispatch_message());
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 1);
    check(dispatch_message());
    check_eq(mail_count(worker), 1u);
    check_eq(*count, 3);
    check(dispatch_message());
    check_eq(mail_count(worker), 0u);
    check_eq(*count, 6);
    check(!dispatch_message());
  }
  SECTION("calling dispatch_messages processes all messages") {
    check_eq(dispatch_messages(), 3u);
    check_eq(*count, 6);
  }
#ifdef CAF_ENABLE_EXCEPTIONS
  SECTION("expect() checks for required messages") {
    expect<int32_t>().to(worker);
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 1);
    expect<int32_t>().to(worker);
    check_eq(mail_count(worker), 1u);
    check_eq(*count, 3);
    expect<int32_t>().to(worker);
    check_eq(mail_count(worker), 0u);
    check_eq(*count, 6);
    should_fail_with_exception([this, worker] { //
      expect<int32_t>().with(3).to(worker);
    });
  }
  SECTION("expect() matches the types of the next message") {
    anon_mail("4").send(worker);
    should_fail_with_exception([this, worker] { //
      expect<std::string>().to(worker);
    });
    should_fail_with_exception([this, worker] { //
      expect<int32_t, int32_t>().to(worker);
    });
    check_eq(*count, 0);
    check_eq(dispatch_messages(), 4u);
    check_eq(*count, 10);
  }
  SECTION("expect() optionally matches the content of the next message") {
    should_fail_with_exception([this, worker] { //
      expect<int32_t>().with(3).to(worker);
    });
    check_eq(*count, 0);
    expect<int32_t>().with(1).to(worker);
    expect<int32_t>().with(2).to(worker);
    expect<int32_t>().with(3).to(worker);
    check_eq(*count, 6);
  }
  SECTION("expect() optionally matches the sender of the next message") {
    // First message has no sender.
    should_fail_with_exception([this, worker, &self] { //
      expect<int32_t>().from(self).to(worker);
    });
    check_eq(*count, 0);
    expect<int32_t>().from(nullptr).to(worker);
    check_eq(*count, 1);
    // Second message is from self.
    should_fail_with_exception([this, worker] { //
      expect<int32_t>().from(nullptr).to(worker);
    });
    check_eq(*count, 1);
    expect<int32_t>().from(self).to(worker);
    check_eq(*count, 3);
  }
  SECTION("prepone_and_expect() processes out of order based on types") {
    anon_mail("4").send(worker);
    prepone_and_expect<std::string>().to(worker);
    check_eq(*count, 4);
    should_fail_with_exception([this, worker] { //
      prepone_and_expect<std::string>().to(worker);
    });
    check_eq(*count, 4);
    check_eq(dispatch_messages(), 3u);
    check_eq(*count, 10);
  }
  SECTION("prepone_and_expect() processes out of order based on content") {
    prepone_and_expect<int32_t>().with(3).to(worker);
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 3);
    should_fail_with_exception([this, worker] { //
      prepone_and_expect<int32_t>().with(3).to(worker);
    });
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 3);
    prepone_and_expect<int32_t>().with(1).to(worker);
    check_eq(*count, 4);
    prepone_and_expect<int32_t>().with(2).to(worker);
    check_eq(*count, 6);
    check(!dispatch_message());
  }
  SECTION("prepone_and_expect() processes out of order based on senders") {
    prepone_and_expect<int32_t>().from(self).to(worker);
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 2);
    should_fail_with_exception([this, worker, &self] { //
      prepone_and_expect<int32_t>().from(self).to(worker);
    });
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 2);
    prepone_and_expect<int32_t>().from(nullptr).to(worker);
    check_eq(*count, 3);
    prepone_and_expect<int32_t>().from(nullptr).to(worker);
    check_eq(*count, 6);
    check(!dispatch_message());
  }
#endif // CAF_ENABLE_EXCEPTIONS
  SECTION("allow() checks for optional messages") {
    check(allow<int32_t>().to(worker));
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 1);
    check(allow<int32_t>().to(worker));
    check_eq(mail_count(worker), 1u);
    check_eq(*count, 3);
    check(allow<int32_t>().to(worker));
    check_eq(mail_count(worker), 0u);
    check_eq(*count, 6);
    check(!allow<int32_t>().with(3).to(worker));
  }
  SECTION("allow() matches the types of the next message") {
    anon_mail("4").send(worker);
    check(!allow<std::string>().to(worker));
    check(!allow<int32_t, int32_t>().to(worker));
    check_eq(*count, 0);
    check_eq(dispatch_messages(), 4u);
    check_eq(*count, 10);
  }
  SECTION("allow() optionally matches the content of the next message") {
    check(!allow<int32_t>().with(3).to(worker));
    check_eq(*count, 0);
    check(allow<int32_t>().with(1).to(worker));
    check(allow<int32_t>().with(2).to(worker));
    check(allow<int32_t>().with(3).to(worker));
    check_eq(*count, 6);
  }
  SECTION("allow() optionally matches the sender of the next message") {
    // First message has no sender.
    check(!allow<int32_t>().from(self).to(worker));
    check_eq(*count, 0);
    check(allow<int32_t>().from(nullptr).to(worker));
    check_eq(*count, 1);
    // Second message is from self.
    check(!allow<int32_t>().from(nullptr).to(worker));
    check_eq(*count, 1);
    check(allow<int32_t>().from(self).to(worker));
    check_eq(*count, 3);
  }
  SECTION("prepone_and_allow() checks for optional messages") {
    check(prepone_and_allow<int32_t>().to(worker));
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 1);
    check(prepone_and_allow<int32_t>().to(worker));
    check_eq(mail_count(worker), 1u);
    check_eq(*count, 3);
    check(prepone_and_allow<int32_t>().to(worker));
    check_eq(mail_count(worker), 0u);
    check_eq(*count, 6);
    check(!prepone_and_allow<int32_t>().with(3).to(worker));
  }
  SECTION("prepone_and_allow() processes out of order based on types") {
    anon_mail("4").send(worker);
    check(prepone_and_allow<std::string>().to(worker));
    check(!prepone_and_allow<std::string>().to(worker));
    check_eq(*count, 4);
    check_eq(dispatch_messages(), 3u);
    check_eq(*count, 10);
  }
  SECTION("prepone_and_allow() processes out of order based on content") {
    check(prepone_and_allow<int32_t>().with(3).to(worker));
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 3);
    check(!prepone_and_allow<int32_t>().with(3).to(worker));
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 3);
    check(prepone_and_allow<int32_t>().with(1).to(worker));
    check_eq(*count, 4);
    check(prepone_and_allow<int32_t>().with(2).to(worker));
    check_eq(*count, 6);
    check(!dispatch_message());
  }
  SECTION("prepone_and_allow() processes out of order based on senders") {
    check(prepone_and_allow<int32_t>().from(self).to(worker));
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 2);
    check(!prepone_and_allow<int32_t>().from(self).to(worker));
    check_eq(mail_count(worker), 2u);
    check_eq(*count, 2);
    check(prepone_and_allow<int32_t>().from(nullptr).to(worker));
    check_eq(*count, 3);
    check(prepone_and_allow<int32_t>().from(nullptr).to(worker));
    check_eq(*count, 6);
    check(!dispatch_message());
  }
  SECTION("prepone_and_allow() ignores non-existent combinations") {
    // There is no message with content (4).
    check(!prepone_and_allow<int32_t>().with(4).from(self).to(worker));
    // There is no message with content (3) from self.
    check(!prepone_and_allow<int32_t>().with(3).from(self).to(worker));
    // The original order should be preserved.
    check(dispatch_message());
    check_eq(*count, 1);
    check(dispatch_message());
    check_eq(*count, 3);
    check(dispatch_message());
    check_eq(*count, 6);
  }
}

TEST("evaluator expressions can check or extract individual values") {
  auto worker = sys.spawn([](caf::event_based_actor* self) -> caf::behavior {
    self->set_default_handler(caf::drop);
    return {
      [](int32_t) {},
    };
  });
  SECTION("omitting with() matches on the types only") {
    anon_mail(1).send(worker);
    check(!allow<std::string>().to(worker));
    check(allow<int>().to(worker));
    anon_mail(1, "two", 3.0).send(worker);
    check(!allow<int>().to(worker));
    check(allow<int, std::string, double>().to(worker));
    anon_mail(42, "hello world", 7.7).send(worker);
    check(allow<int, std::string, double>().to(worker));
  }
  SECTION("reference wrappers turn evaluators into extractors") {
    auto tmp = 0;
    anon_mail(1).send(worker);
    check(allow<int>().with(std::ref(tmp)).to(worker));
    check_eq(tmp, 1);
  }
  SECTION("std::ignore matches any value") {
    anon_mail(1).send(worker);
    check(allow<int>().with(std::ignore).to(worker));
    anon_mail(2).send(worker);
    check(allow<int>().with(std::ignore).to(worker));
    anon_mail(3).send(worker);
    check(allow<int>().with(std::ignore).to(worker));
    anon_mail(1, 2, 3).send(worker);
    check(!allow<int, int, int>().with(1, std::ignore, 4).to(worker));
    check(!allow<int, int, int>().with(2, std::ignore, 3).to(worker));
    check(allow<int, int, int>().with(1, std::ignore, 3).to(worker));
  }
  SECTION("value predicates allow user-defined types") {
    anon_mail(my_int{1}).send(worker);
    check(allow<my_int>().to(worker));
    anon_mail(my_int{1}).send(worker);
    check(allow<my_int>().with(std::ignore).to(worker));
    anon_mail(my_int{1}).send(worker);
    check(!allow<my_int>().with(my_int{2}).to(worker));
    check(allow<my_int>().with(my_int{1}).to(worker));
    anon_mail(my_int{1}).send(worker);
    check(allow<my_int>().with(1).to(worker));
    auto tmp = my_int{0};
    anon_mail(my_int{42}).send(worker);
    check(allow<my_int>().with(std::ref(tmp)).to(worker));
    check_eq(tmp.value, 42);
  }
  SECTION("value predicates allow user-defined predicates") {
    auto le2 = [](my_int x) { return x.value <= 2; };
    anon_mail(my_int{1}).send(worker);
    check(allow<my_int>().with(le2).to(worker));
    anon_mail(my_int{2}).send(worker);
    check(allow<my_int>().with(le2).to(worker));
    anon_mail(my_int{3}).send(worker);
    check(!allow<my_int>().with(le2).to(worker));
  }
}

SCENARIO("the deterministic fixture allows setting the actor clock at will") {
  caf::chrono::datetime caf_epoch_dt; // Date and time of the first CAF commit.
  if (auto err = caf::chrono::parse("2011-03-04T16:03:40+0100", caf_epoch_dt)) {
    fail("failed to parse datetime: {}", to_string(err));
  }
  using time_point = caf::actor_clock::time_point;
  auto to_timestamp = [](time_point tp) {
    return caf::timestamp{tp.time_since_epoch()};
  };
  auto caf_epoch = time_point{caf_epoch_dt.to_local_time().time_since_epoch()};
  auto& clock = sys.clock();
  require_le(clock.now(), caf_epoch);
  WHEN("scheduling an action") {
    THEN("CAF will create a pending timeout") {
      check_eq(num_timeouts(), 0u);
      clock.schedule(caf::make_action([] {}));
      check_eq(num_timeouts(), 1u);
      clock.schedule(caf::make_action([] {}));
      check_eq(num_timeouts(), 2u);
    }
  }
  WHEN("scheduling an action with a time point") {
    THEN("CAF will create a pending timeout") {
      auto t = clock.now();
      check_eq(num_timeouts(), 0u);
      clock.schedule(t, caf::make_action([] {}));
      check_eq(num_timeouts(), 1u);
      check_eq(to_timestamp(next_timeout()), to_timestamp(t));
      check_eq(to_timestamp(last_timeout()), to_timestamp(t));
      auto last = clock.schedule(t + 5s, caf::make_action([] {}));
      check_eq(num_timeouts(), 2u);
      check_eq(to_timestamp(next_timeout()), to_timestamp(t));
      check_eq(to_timestamp(last_timeout()), to_timestamp(t + 5s));
      clock.schedule(t + 3s, caf::make_action([] {}));
      check_eq(num_timeouts(), 3u);
      check_eq(to_timestamp(next_timeout()), to_timestamp(t));
      check_eq(to_timestamp(last_timeout()), to_timestamp(t + 5s));
      last.dispose();
      check_eq(num_timeouts(), 2u);
      check_eq(to_timestamp(next_timeout()), to_timestamp(t));
      check_eq(to_timestamp(last_timeout()), to_timestamp(t + 3s));
    }
  }
  WHEN("calling set_time with a time point after the current time") {
    auto triggered = std::make_shared<bool>(false);
    clock.schedule(caf::make_action([triggered] { *triggered = true; }));
    THEN("the clock advances to the new time and timeouts will trigger") {
      check_eq(set_time(caf_epoch), 1u);
      check_eq(to_timestamp(clock.now()), to_timestamp(caf_epoch));
      check(*triggered);
    }
  }
  WHEN("calling set_time with a time point prior to the current time") {
    auto t = caf_epoch - 8766h;
    set_time(caf_epoch);
    auto triggered = std::make_shared<bool>(false);
    clock.schedule(caf::make_action([triggered] { *triggered = true; }));
    THEN("the clock rewinds to the new time and no timeouts will trigger") {
      check_eq(set_time(t), 0u);
      check_eq(to_timestamp(clock.now()), to_timestamp(t));
      check(!*triggered);
    }
  }
  WHEN("calling set_time with the current time") {
    auto triggered = std::make_shared<bool>(false);
    clock.schedule(caf::make_action([triggered] { *triggered = true; }));
    THEN("nothing changes and no timeouts will trigger") {
      auto now = clock.now();
      check_eq(set_time(now), 0u);
      check_eq(to_timestamp(clock.now()), to_timestamp(now));
      check(!*triggered);
    }
  }
  WHEN("calling advance_time") {
    THEN("the clock advances to the new time and timeouts will trigger") {
      set_time(caf_epoch);
      auto a1 = std::make_shared<bool>(false);
      clock.schedule(caf_epoch + 1s, caf::make_action([a1] { *a1 = true; }));
      auto a2 = std::make_shared<bool>(false);
      clock.schedule(caf_epoch + 2s, caf::make_action([a2] { *a2 = true; }));
      auto a3 = std::make_shared<bool>(false);
      clock.schedule(caf_epoch + 3s, caf::make_action([a3] { *a3 = true; }));
      check_eq(advance_time(2s), 2u);
      check_eq(to_timestamp(clock.now()), to_timestamp(caf_epoch + 2s));
      check(*a1);
      check(*a2);
      check(!*a3);
    }
  }
  WHEN("calling trigger_timeout") {
    THEN("the next pending timeout will trigger and the time advances") {
      set_time(caf_epoch);
      auto a1 = std::make_shared<bool>(false);
      clock.schedule(caf_epoch + 1s, caf::make_action([a1] { *a1 = true; }));
      auto a2 = std::make_shared<bool>(false);
      clock.schedule(caf_epoch + 2s, caf::make_action([a2] { *a2 = true; }));
      auto a3 = std::make_shared<bool>(false);
      clock.schedule(caf_epoch + 3s, caf::make_action([a3] { *a3 = true; }));
      check(trigger_timeout());
      check_eq(to_timestamp(clock.now()), to_timestamp(caf_epoch + 1s));
      check(*a1);
      check(!*a2);
      check(!*a3);
    }
  }
  WHEN("calling trigger_all_timeouts with pending timeouts in the future") {
    THEN("all pending timeouts will trigger and the time advances") {
      set_time(caf_epoch);
      auto a1 = std::make_shared<bool>(false);
      clock.schedule(caf_epoch + 1s, caf::make_action([a1] { *a1 = true; }));
      auto a2 = std::make_shared<bool>(false);
      clock.schedule(caf_epoch + 2s, caf::make_action([a2] { *a2 = true; }));
      auto a3 = std::make_shared<bool>(false);
      clock.schedule(caf_epoch + 3s, caf::make_action([a3] { *a3 = true; }));
      check_eq(trigger_all_timeouts(), 3u);
      check_eq(to_timestamp(clock.now()), to_timestamp(caf_epoch + 3s));
      check(*a1);
      check(*a2);
      check(*a3);
    }
  }
  WHEN("calling trigger_all_timeouts with pending timeouts in the past") {
    THEN("all pending timeouts will trigger but the time stays the same") {
      auto t = clock.now();
      set_time(caf_epoch);
      auto a1 = std::make_shared<bool>(false);
      clock.schedule(t, caf::make_action([a1] { *a1 = true; }));
      auto a2 = std::make_shared<bool>(false);
      clock.schedule(t, caf::make_action([a2] { *a2 = true; }));
      auto a3 = std::make_shared<bool>(false);
      clock.schedule(t, caf::make_action([a3] { *a3 = true; }));
      check_eq(trigger_all_timeouts(), 3u);
      check_eq(to_timestamp(clock.now()), to_timestamp(caf_epoch));
      check(*a1);
      check(*a2);
      check(*a3);
    }
  }
}

#ifdef CAF_ENABLE_EXCEPTIONS
TEST("advance_time requires a positive duration") {
  should_fail_with_exception([this] { advance_time(0s); });
  should_fail_with_exception([this] { advance_time(-1s); });
}

TEST("calling next_timeout or last_timeout with no pending timeout throws") {
  should_fail_with_exception([this] { std::ignore = next_timeout(); });
  should_fail_with_exception([this] { std::ignore = last_timeout(); });
}
#endif // CAF_ENABLE_EXCEPTIONS

} // WITH_FIXTURE(caf::test::fixture::deterministic)

TEST_INIT() {
  caf::init_global_meta_objects<caf::id_block::deterministic_fixture_test>();
}
