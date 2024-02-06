// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/anon_mail.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/behavior.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message_priority.hpp"

using namespace caf;
using namespace std::literals;

namespace {

WITH_FIXTURE(test::fixture::deterministic) {

TEST("send asynchronous message") {
  auto dummy = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [=](const std::string&) {},
    };
  });
  SECTION("regular message") {
    anon_mail("hello world").send(dummy);
    expect<std::string>()
      .with("hello world")
      .priority(message_priority::normal)
      .from(nullptr)
      .to(dummy);
  }
  SECTION("urgent message") {
    anon_mail("hello world").urgent().send(dummy);
    expect<std::string>()
      .with("hello world")
      .priority(message_priority::high)
      .from(nullptr)
      .to(dummy);
  }
  SECTION("invalid receiver") {
    anon_mail("hello world").urgent().send(actor{});
    check_eq(mail_count(), 0u);
  }
}

TEST("send delayed message") {
  auto dummy = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [=](const std::string&) {},
    };
  });
  SECTION("regular message") {
    SECTION("strong reference to the receiver") {
      anon_mail("hello world").delay(1s).send(dummy);
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 1u);
      trigger_timeout();
      expect<std::string>()
        .with("hello world")
        .priority(message_priority::normal)
        .from(nullptr)
        .to(dummy);
    }
    SECTION("weak reference to the receiver") {
      anon_mail("hello world").delay(1s).send(dummy, weak_ref);
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 1u);
      trigger_timeout();
      expect<std::string>()
        .with("hello world")
        .priority(message_priority::normal)
        .from(nullptr)
        .to(dummy);
    }
  }
  SECTION("urgent message") {
    SECTION("strong reference to the receiver") {
      anon_mail("hello world").urgent().delay(1s).send(dummy, strong_ref);
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 1u);
      trigger_timeout();
      expect<std::string>()
        .with("hello world")
        .priority(message_priority::high)
        .from(nullptr)
        .to(dummy);
    }
    SECTION("weak reference to the receiver") {
      anon_mail("hello world").urgent().delay(1s).send(dummy, weak_ref);
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 1u);
      trigger_timeout();
      expect<std::string>()
        .with("hello world")
        .priority(message_priority::high)
        .from(nullptr)
        .to(dummy);
    }
    SECTION("invalid receiver") {
      anon_mail("hello world").delay(1s).send(actor{});
      check_eq(mail_count(), 0u);
    }
  }
}

TEST("implicit cancel of a delayed message") {
  auto dummy = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [=](const std::string&) {},
    };
  });
  anon_mail("hello world").delay(1s).send(dummy, weak_ref);
  dummy = nullptr;
  check_eq(mail_count(), 0u);
  check_eq(num_timeouts(), 1u);
  trigger_timeout();
  check_eq(mail_count(), 0u);
  check_eq(num_timeouts(), 0u);
}

TEST("explicit cancel of a delayed message") {
  auto dummy = sys.spawn([](event_based_actor*) -> behavior {
    return {
      [=](const std::string&) {},
    };
  });
  SECTION("strong reference to the receiver") {
    auto hdl = anon_mail("hello world").delay(1s).send(dummy, strong_ref);
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 1u);
    hdl.dispose();
    trigger_timeout();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 0u);
  }
  SECTION("weak reference to the receiver") {
    auto hdl = anon_mail("hello world").delay(1s).send(dummy, weak_ref);
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 1u);
    hdl.dispose();
    trigger_timeout();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 0u);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
