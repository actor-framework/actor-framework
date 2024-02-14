// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/event_based_mail.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/dynamically_typed.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

// Note: functions inherited from async_mail are tested in async_mail.test.cpp.

using namespace caf;
using namespace std::literals;

namespace {

using dummy_actor = typed_actor<result<int>(int)>;

using dummy_behavior = dummy_actor::behavior_type;

WITH_FIXTURE(test::fixture::deterministic) {

TEST("send request message") {
  auto [self, launch] = sys.spawn_inactive<event_based_actor>();
  auto self_hdl = actor_cast<actor>(self);
  SECTION("using .then for the response") {
    SECTION("valid response") {
      auto dummy = sys.spawn([]() -> dummy_behavior {
        return {
          [](int value) { return value * value; },
        };
      });
      auto result = std::make_shared<int>(0);
      SECTION("regular message") {
        self->mail(3).request(dummy).then([result](int x) { *result = x; });
        launch();
        expect<int>()
          .with(3)
          .priority(message_priority::normal)
          .from(self_hdl)
          .to(dummy);
        expect<int>()
          .with(9)
          .priority(message_priority::normal)
          .from(dummy)
          .to(self_hdl);
        check_eq(*result, 9);
      }
      SECTION("urgent message") {
        self->mail(3)
          .urgent()
          .request(dummy) //
          .then([result](int x) { *result = x; });
        launch();
        expect<int>()
          .with(3)
          .priority(message_priority::high)
          .from(self_hdl)
          .to(dummy);
        expect<int>()
          .with(9)
          .priority(message_priority::high)
          .from(dummy)
          .to(self_hdl);
        check_eq(*result, 9);
      }
    }
    SECTION("invalid response") {
      auto dummy = sys.spawn([](event_based_actor*) -> behavior {
        return {
          [](int) { return "ok"s; },
        };
      });
      auto result = std::make_shared<error>();
      self->mail(3)
        .request(dummy) //
        .then([this](int value) { fail("expected a string, got: {}", value); },
              [result](error& err) { *result = std::move(err); });
      launch();
      expect<int>()
        .with(3)
        .priority(message_priority::normal)
        .from(self_hdl)
        .to(dummy);
      expect<std::string>()
        .priority(message_priority::normal)
        .from(dummy)
        .to(self_hdl);
      check_eq(*result, make_error(sec::unexpected_response));
    }
    SECTION("invalid receiver") {
      auto result = std::make_shared<error>();
      self->mail(3)
        .request(actor{}) //
        .then([this](int value) { fail("expected a string, got: {}", value); },
              [result](error& err) { *result = std::move(err); });
      check_eq(mail_count(), 1u);
      launch();
      expect<error>().to(self_hdl);
      check_eq(*result, make_error(sec::invalid_request));
    }
  }
  SECTION("using .await for the response") {
    SECTION("valid response") {
      auto dummy = sys.spawn([]() -> dummy_behavior {
        return {
          [](int value) { return value * value; },
        };
      });
      auto result = std::make_shared<int>(0);
      SECTION("regular message") {
        self->mail(3).request(dummy).await([result](int x) { *result = x; });
        launch();
        expect<int>()
          .with(3)
          .priority(message_priority::normal)
          .from(self_hdl)
          .to(dummy);
        expect<int>()
          .with(9)
          .priority(message_priority::normal)
          .from(dummy)
          .to(self_hdl);
        check_eq(*result, 9);
      }
      SECTION("urgent message") {
        self->mail(3)
          .urgent()
          .request(dummy) //
          .await([result](int x) { *result = x; });
        launch();
        expect<int>()
          .with(3)
          .priority(message_priority::high)
          .from(self_hdl)
          .to(dummy);
        expect<int>()
          .with(9)
          .priority(message_priority::high)
          .from(dummy)
          .to(self_hdl);
        check_eq(*result, 9);
      }
    }
    SECTION("invalid response") {
      auto dummy = sys.spawn([](event_based_actor*) -> behavior {
        return {
          [](int) { return "ok"s; },
        };
      });
      auto result = std::make_shared<error>();
      self->mail(3)
        .request(dummy) //
        .await([this](int value) { fail("expected a string, got: {}", value); },
               [result](error& err) { *result = std::move(err); });
      launch();
      expect<int>()
        .with(3)
        .priority(message_priority::normal)
        .from(self_hdl)
        .to(dummy);
      expect<std::string>()
        .priority(message_priority::normal)
        .from(dummy)
        .to(self_hdl);
      check_eq(*result, make_error(sec::unexpected_response));
    }
    SECTION("invalid receiver") {
      auto result = std::make_shared<error>();
      self->mail(3)
        .request(actor{}) //
        .await([this](int value) { fail("expected a string, got: {}", value); },
               [result](error& err) { *result = std::move(err); });
      check_eq(mail_count(), 1u);
      launch();
      expect<error>().to(self_hdl);
      check_eq(*result, make_error(sec::invalid_request));
    }
  }
}

TEST("send delayed request message") {
  auto [self, launch] = sys.spawn_inactive<event_based_actor>();
  auto self_hdl = actor_cast<actor>(self);
  auto dummy = sys.spawn([]() -> dummy_behavior {
    return {
      [](int value) { return value * value; },
    };
  });
  auto result = std::make_shared<int>(0);
  auto on_result = [result](int value) { *result = value; };
  SECTION("strong receiver reference") {
    self->mail(3).delay(1s).request(dummy, strong_ref).first.then(on_result);
    launch();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 1u);
    trigger_timeout();
    expect<int>()
      .with(3)
      .priority(message_priority::normal)
      .from(self_hdl)
      .to(dummy);
    expect<int>()
      .with(9)
      .priority(message_priority::normal)
      .from(dummy)
      .to(self_hdl);
    check_eq(*result, 9);
  }
  SECTION("weak receiver reference") {
    self->mail(3)
      .schedule(sys.clock().now() + 1s)
      .request(dummy, weak_ref)
      .first.then(on_result);
    launch();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 1u);
    trigger_timeout();
    expect<int>()
      .with(3)
      .priority(message_priority::normal)
      .from(self_hdl)
      .to(dummy);
    expect<int>()
      .with(9)
      .priority(message_priority::normal)
      .from(dummy)
      .to(self_hdl);
    check_eq(*result, 9);
  }
  SECTION("weak receiver reference that expires") {
    self->mail(3)
      .schedule(sys.clock().now() + 1s)
      .request(dummy, weak_ref)
      .first.then(on_result);
    launch();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 1u);
    dummy = nullptr;
    trigger_timeout();
    expect<error>().with(make_error(sec::request_receiver_down)).to(self_hdl);
  }
  SECTION("weak sender reference that expires") {
    self->mail(3)
      .schedule(sys.clock().now() + 1s)
      .request(dummy, strong_ref, weak_self_ref)
      .first.then(on_result);
    launch();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 1u);
    self_hdl = nullptr;
    trigger_timeout();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 0u);
  }
}

TEST("send request message as a typed actor") {
  using sender_actor = typed_actor<result<void>(int)>;
  auto dummy = sys.spawn([]() -> dummy_behavior {
    return {
      [](int value) { return value * value; },
    };
  });
  auto [self, launch] = sys.spawn_inactive<sender_actor::impl>();
  auto self_hdl = actor_cast<actor>(self);
  auto result = std::make_shared<int>(0);
  self->mail(3).request(dummy).then([result](int x) { *result = x; });
  launch();
  expect<int>()
    .with(3)
    .priority(message_priority::normal)
    .from(self_hdl)
    .to(dummy);
  expect<int>()
    .with(9)
    .priority(message_priority::normal)
    .from(dummy)
    .to(self_hdl);
  check_eq(*result, 9);
}

TEST("send request message to an invalid receiver") {
  auto dummy = sys.spawn([]() -> dummy_behavior {
    return {
      [](int value) { return value * value; },
    };
  });
  auto [self, launch] = sys.spawn_inactive<event_based_actor>();
  auto self_hdl = actor_cast<actor>(self);
  auto result = std::make_shared<int>(0);
  auto on_result = [result](int x) { *result = x; };
  auto err = std::make_shared<error>();
  auto on_error = [err](error x) { *err = x; };
  SECTION("regular message") {
    self->mail("hello world").request(actor{}).then(on_result, on_error);
    launch();
    expect<error>().with(make_error(sec::invalid_request)).to(self_hdl);
    check_eq(*result, 0);
    check_eq(*err, make_error(sec::invalid_request));
  }
  SECTION("delayed message") {
    self->mail("hello world")
      .delay(1s)
      .request(actor{})
      .first.then(on_result, on_error);
    launch();
    check_eq(mail_count(), 1u);
    check_eq(num_timeouts(), 0u);
    expect<error>().with(make_error(sec::invalid_request)).to(self_hdl);
    check_eq(*result, 0);
    check_eq(*err, make_error(sec::invalid_request));
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
