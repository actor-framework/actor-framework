// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/send.hpp"

namespace fixture = caf::test::fixture;

template <class T>
using value_predicate = fixture::deterministic::value_predicate<T>;

template <class... Ts>
using message_predicate = fixture::deterministic::message_predicate<Ts...>;

struct my_int {
  int value;
};

bool operator==(my_int lhs, int rhs) noexcept {
  return lhs.value == rhs;
}

bool operator==(int lhs, my_int rhs) noexcept {
  return lhs == rhs.value;
}

TEST("value predicates check or extract individual values") {
  using predicate_t = value_predicate<int>;
  SECTION("catch-all predicates are constructible from std::ignore") {
    predicate_t f{std::ignore};
    check(f(1));
    check(f(2));
    check(f(3));
  }

  SECTION("a default-constructed predicate always returns true") {
    predicate_t f;
    check(f(1));
    check(f(2));
    check(f(3));
  }
  SECTION("exact match predicates are constructible from a value") {
    predicate_t f{2};
    check(!f(1));
    check(f(2));
    check(!f(3));
  }
  SECTION("exact match predicates are constructible from any comparable type") {
    predicate_t f{my_int{2}};
    check(!f(1));
    check(f(2));
    check(!f(3));
  }
  SECTION("custom predicates are constructible from a function object") {
    predicate_t f{[](int x) { return x <= 2; }};
    check(f(1));
    check(f(2));
    check(!f(3));
  }
  SECTION("extractors are constructible from a reference wrapper") {
    int x = 0;
    predicate_t f{std::ref(x)};
    check(f(1)) && check_eq(x, 1);
    check(f(2)) && check_eq(x, 2);
    check(f(3)) && check_eq(x, 3);
  }
}

TEST("message predicates check all values in a message") {
  using predicate_t = message_predicate<int, std::string, double>;
  SECTION("a default-constructed message predicate matches anything") {
    predicate_t f;
    check(f(caf::make_message(1, "two", 3.0)));
    check(f(caf::make_message(42, "hello world", 7.7)));
  }
  SECTION("message predicates can match values") {
    predicate_t f{1, "two", [](double x) { return x < 5.0; }};
    check(f(caf::make_message(1, "two", 3.0)));
    check(!f(caf::make_message(1, "two", 6.0)));
  }
  SECTION("message predicates can extract values") {
    auto x0 = 0;
    auto x1 = std::string{};
    auto x2 = 0.0;
    predicate_t f{std::ref(x0), std::ref(x1), std::ref(x2)};
    check(f(caf::make_message(1, "two", 3.0)));
    check_eq(x0, 1);
    check_eq(x1, "two");
    check_eq(x2, 3.0);
  }
}

WITH_FIXTURE(fixture::deterministic) {

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
  anon_send(worker, 1);
  check_eq(mail_count(worker), 1u);
  self->send(worker, 2);
  check_eq(mail_count(worker), 2u);
  anon_send(worker, 3);
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
    anon_send(worker, "4");
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
    anon_send(worker, "4");
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
    anon_send(worker, "4");
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
    anon_send(worker, "4");
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

} // WITH_FIXTURE(fixture::deterministic)

CAF_TEST_MAIN()
