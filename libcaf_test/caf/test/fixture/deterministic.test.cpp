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

bool operator==(int lhs, my_int rhs) noexcept {
  return lhs == rhs.value;
}

bool operator!=(my_int lhs, my_int rhs) noexcept {
  return lhs.value != rhs.value;
}

CAF_BEGIN_TYPE_ID_BLOCK(test, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(test, (my_int))

CAF_END_TYPE_ID_BLOCK(test)

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

TEST("evaluator expressions can check or extract individual values") {
  auto worker = sys.spawn([](caf::event_based_actor* self) -> caf::behavior {
    self->set_default_handler(caf::drop);
    return {
      [](int32_t) {},
    };
  });
  SECTION("omitting with() matches on the types only") {
    anon_send(worker, 1);
    check(!allow<std::string>().to(worker));
    check(allow<int>().to(worker));
    anon_send(worker, 1, "two", 3.0);
    check(!allow<int>().to(worker));
    check(allow<int, std::string, double>().to(worker));
    anon_send(worker, 42, "hello world", 7.7);
    check(allow<int, std::string, double>().to(worker));
  }
  SECTION("reference wrappers turn evaluators into extractors") {
    auto tmp = 0;
    anon_send(worker, 1);
    check(allow<int>().with(std::ref(tmp)).to(worker));
    check_eq(tmp, 1);
  }
  SECTION("std::ignore matches any value") {
    anon_send(worker, 1);
    check(allow<int>().with(std::ignore).to(worker));
    anon_send(worker, 2);
    check(allow<int>().with(std::ignore).to(worker));
    anon_send(worker, 3);
    check(allow<int>().with(std::ignore).to(worker));
    anon_send(worker, 1, 2, 3);
    check(!allow<int, int, int>().with(1, std::ignore, 4).to(worker));
    check(!allow<int, int, int>().with(2, std::ignore, 3).to(worker));
    check(allow<int, int, int>().with(1, std::ignore, 3).to(worker));
  }
  SECTION("value predicates allow user-defined types") {
    anon_send(worker, my_int{1});
    check(allow<my_int>().to(worker));
    anon_send(worker, my_int{1});
    check(allow<my_int>().with(std::ignore).to(worker));
    anon_send(worker, my_int{1});
    check(!allow<my_int>().with(my_int{2}).to(worker));
    check(allow<my_int>().with(my_int{1}).to(worker));
    anon_send(worker, my_int{1});
    check(allow<my_int>().with(1).to(worker));
    auto tmp = my_int{0};
    anon_send(worker, my_int{42});
    check(allow<my_int>().with(std::ref(tmp)).to(worker));
    check_eq(tmp.value, 42);
  }
  SECTION("value predicates allow user-defined predicates") {
    auto le2 = [](my_int x) { return x.value <= 2; };
    anon_send(worker, my_int{1});
    check(allow<my_int>().with(le2).to(worker));
    anon_send(worker, my_int{2});
    check(allow<my_int>().with(le2).to(worker));
    anon_send(worker, my_int{3});
    check(!allow<my_int>().with(le2).to(worker));
  }
}

} // WITH_FIXTURE(fixture::deterministic)

CAF_TEST_MAIN(caf::id_block::test)
