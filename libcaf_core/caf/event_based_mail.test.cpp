// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/event_based_mail.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/disposable.hpp"
#include "caf/dynamically_typed.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/timespan.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

// Note: functions inherited from async_mail are tested in async_mail.test.cpp.

using namespace caf;
using namespace std::literals;

using dummy_actor = typed_actor<result<int>(int)>;

using dummy_behavior = dummy_actor::behavior_type;

WITH_FIXTURE(test::fixture::deterministic) {

TEST("send request message") {
  auto [self, launch] = sys.spawn_inactive();
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
        self->mail(3)
          .request(dummy, infinite) //
          .then([result](int x) { *result = x; });
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
          .request(dummy, infinite) //
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
        .request(dummy, infinite) //
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
        .request(actor{}, 1s) //
        .then([this](int value) { fail("expected a string, got: {}", value); },
              [result](error& err) { *result = std::move(err); });
      check_eq(mail_count(), 1u);
      launch();
      expect<error>().to(self_hdl);
      check_eq(*result, make_error(sec::invalid_request));
    }
    SECTION("no response") {
      auto result = std::make_shared<error>();
      auto dummy = sys.spawn([](event_based_actor* self) -> behavior {
        auto res = std::make_shared<response_promise>();
        return {
          [self, res](int) { *res = self->make_response_promise(); },
        };
      });
      self->mail(3)
        .request(dummy, 1s) //
        .then([this](int) { fail("unexpected response"); },
              [result](error& err) { *result = std::move(err); });
      launch();
      check_eq(mail_count(), 1u);
      check_eq(num_timeouts(), 1u);
      expect<int>()
        .with(3)
        .priority(message_priority::normal)
        .from(self_hdl)
        .to(dummy);
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 1u);
      trigger_timeout();
      expect<error>().with(make_error(sec::request_timeout)).to(self_hdl);
      check_eq(*result, make_error(sec::request_timeout));
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 0u);
      self->mail(exit_msg{nullptr, exit_reason::user_shutdown}).send(dummy);
      expect<exit_msg>().to(dummy);
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
        self->mail(3)
          .request(dummy, infinite) //
          .await([result](int x) { *result = x; });
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
          .request(dummy, infinite) //
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
        .request(dummy, infinite) //
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
        .request(actor{}, 1s) //
        .await([this](int value) { fail("expected a string, got: {}", value); },
               [result](error& err) { *result = std::move(err); });
      check_eq(mail_count(), 1u);
      launch();
      expect<error>().to(self_hdl);
      check_eq(*result, make_error(sec::invalid_request));
    }
    SECTION("no response") {
      auto result = std::make_shared<error>();
      auto dummy = sys.spawn([](event_based_actor* self) -> behavior {
        auto res = std::make_shared<response_promise>();
        return {
          [self, res](int) { *res = self->make_response_promise(); },
        };
      });
      self->mail(3)
        .request(dummy, 1s) //
        .await([this](int) { fail("unexpected response"); },
               [result](error& err) { *result = std::move(err); });
      launch();
      expect<int>()
        .with(3)
        .priority(message_priority::normal)
        .from(self_hdl)
        .to(dummy);
      check_eq(mail_count(), 0u);
      check_eq(num_timeouts(), 1u);
      trigger_timeout();
      expect<error>().with(make_error(sec::request_timeout)).to(self_hdl);
      check_eq(*result, make_error(sec::request_timeout));
      self->mail(exit_msg{nullptr, exit_reason::user_shutdown}).send(dummy);
      expect<exit_msg>().to(dummy);
    }
  }
  SECTION("using .to_observable for the response") {
    SECTION("response from dynamically typed receiver") {
      auto dummy = sys.spawn([]() -> behavior {
        return {
          [](int value) { return value * value; },
        };
      });
      auto err = std::make_shared<error>();
      auto result = std::make_shared<int>(0);
      SECTION("valid response") {
        self->mail(3)
          .request(dummy, infinite)
          .as_observable<int>()
          .do_on_error([err](const error& x) { *err = x; })
          .for_each([result](int x) { *result = x; });
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
        check_eq(*err, error{});
        check_eq(*result, 9);
      }
      SECTION("error response") {
        self->mail("hello")
          .request(dummy, infinite)
          .as_observable<int>()
          .do_on_error([err](const error& x) { *err = x; })
          .for_each([result](int x) { *result = x; });
        launch();
        expect<std::string>()
          .with("hello")
          .priority(message_priority::normal)
          .from(self_hdl)
          .to(dummy);
        expect<error>()
          .priority(message_priority::normal)
          .from(dummy)
          .to(self_hdl);
        check_eq(*err, error{sec::unexpected_message});
        check_eq(*result, 0);
      }
    }
    SECTION("valid response with typed receiver") {
      auto dummy = sys.spawn([]() -> dummy_behavior {
        return {
          [](int value) { return value * value; },
        };
      });
      auto err = std::make_shared<error>();
      auto result = std::make_shared<int>(0);
      self->mail(3)
        .request(dummy, infinite)
        .as_observable()
        .do_on_error([err](const error& x) { *err = x; })
        .for_each([result](int x) { *result = x; });
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
      check_eq(*err, error{});
      check_eq(*result, 9);
    }
  }
}

TEST("send delayed request message") {
  auto [self, launch] = sys.spawn_inactive();
  auto self_hdl = actor_cast<actor>(self);
  auto dummy = sys.spawn([]() -> dummy_behavior {
    return {
      [](int value) { return value * value; },
    };
  });
  auto result = std::make_shared<int>(0);
  auto on_result = [result](int value) { *result = value; };
  SECTION("strong receiver reference") {
    auto [hdl, pending] = self->mail(3).delay(1s).request(dummy, infinite,
                                                          strong_ref);
    static_assert(
      std::is_same_v<decltype(hdl), event_based_response_handle<int>>);
    static_assert(std::is_same_v<decltype(pending), disposable>);
    std::move(hdl).then(on_result);
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
      .request(dummy, infinite, weak_ref)
      .then(on_result);
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
      .request(dummy, infinite, weak_ref)
      .then(on_result);
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
      .request(dummy, infinite, strong_ref, weak_self_ref)
      .then(on_result);
    launch();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 1u);
    self_hdl = nullptr;
    trigger_timeout();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 0u);
  }
}

TEST("send delayed request message with no response") {
  auto [self, launch] = sys.spawn_inactive();
  auto self_hdl = actor_cast<actor>(self);
  auto result = std::make_shared<error>();
  auto dummy = sys.spawn([](event_based_actor* self) -> behavior {
    auto res = std::make_shared<response_promise>();
    return {
      [self, res](int) { *res = self->make_response_promise(); },
    };
  });
  auto pending = self->mail(3)
                   .delay(1s)
                   .request(dummy, 1s) //
                   .then([this](int) { fail("unexpected response"); },
                         [result](error& err) { *result = std::move(err); });
  static_assert(std::is_same_v<decltype(pending), disposable>);
  launch();
  check_eq(mail_count(), 0u);
  check_eq(num_timeouts(), 2u);
  advance_time(1s);
  check_eq(mail_count(), 1u);
  check_eq(num_timeouts(), 1u);
  expect<int>()
    .with(3)
    .priority(message_priority::normal)
    .from(self_hdl)
    .to(dummy);
  check_eq(mail_count(), 0u);
  check_eq(num_timeouts(), 1u);
  advance_time(1s);
  expect<error>().with(make_error(sec::request_timeout)).to(self_hdl);
  check_eq(*result, make_error(sec::request_timeout));
  self->mail(exit_msg{nullptr, exit_reason::user_shutdown}).send(dummy);
  expect<exit_msg>().to(dummy);
}

TEST("send request message as a typed actor") {
  using sender_actor = typed_actor<result<void>(int)>;
  auto dummy = sys.spawn([]() -> dummy_behavior {
    return {
      [](int value) { return value * value; },
    };
  });
  auto result = std::make_shared<int>(0);
  auto sender = sys.spawn([dummy, result](sender_actor::pointer self) {
    self->mail(3).send(dummy);
    return sender_actor::behavior_type{
      [result](int x) { *result = x; },
    };
  });
  expect<int>()
    .with(3)
    .priority(message_priority::normal)
    .from(sender)
    .to(dummy);
  expect<int>()
    .with(9)
    .priority(message_priority::normal)
    .from(dummy)
    .to(sender);
  check_eq(*result, 9);
}

TEST("send request message to an invalid receiver") {
  auto dummy = sys.spawn([]() -> dummy_behavior {
    return {
      [](int value) { return value * value; },
    };
  });
  auto [self, launch] = sys.spawn_inactive();
  auto self_hdl = actor_cast<actor>(self);
  auto result = std::make_shared<int>(0);
  auto on_result = [result](int x) { *result = x; };
  auto err = std::make_shared<error>();
  auto on_error = [err](error x) { *err = x; };
  SECTION("regular message") {
    self->mail("hello world").request(actor{}, 1s).then(on_result, on_error);
    launch();
    expect<error>().with(make_error(sec::invalid_request)).to(self_hdl);
    check_eq(*result, 0);
    check_eq(*err, make_error(sec::invalid_request));
  }
  SECTION("delayed message") {
    self->mail("hello world")
      .delay(1s)
      .request(actor{}, 1s)
      .then(on_result, on_error);
    launch();
    check_eq(mail_count(), 1u);
    check_eq(num_timeouts(), 0u);
    expect<error>().with(make_error(sec::invalid_request)).to(self_hdl);
    check_eq(*result, 0);
    check_eq(*err, make_error(sec::invalid_request));
  }
}

TEST("delegate message") {
  SECTION("asynchronous message") {
    auto [self, launch] = sys.spawn_inactive();
    auto delegatee = sys.spawn([](event_based_actor*) -> behavior {
      return {
        [=](const std::string&) {},
      };
    });
    SECTION("delegate with default priority") {
      auto delegator = sys.spawn([delegatee](event_based_actor* self) {
        return behavior{
          [=](std::string& str) {
            return self->mail(std::move(str)).delegate(delegatee);
          },
        };
      });
      SECTION("regular message") {
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegatee);
      }
      SECTION("urgent message") {
        self->mail("hello world").urgent().send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
    }
    SECTION("delegate with high priority") {
      auto delegator = sys.spawn([delegatee](event_based_actor* self) {
        return behavior{
          [=](std::string& str) {
            return self->mail(std::move(str)).urgent().delegate(delegatee);
          },
        };
      });
      SECTION("regular message") {
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
      SECTION("urgent message") {
        self->mail("hello world").urgent().send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
    }
  }
  SECTION("request message") {
    auto [self, launch] = sys.spawn_inactive();
    SECTION("delegate with default priority") {
      auto delegatee = sys.spawn([](event_based_actor*) {
        return behavior{
          [=](const std::string& str) {
            return std::string{str.rbegin(), str.rend()};
          },
        };
      });
      auto delegator = sys.spawn([delegatee](event_based_actor* self) {
        return behavior{
          [=](std::string& str) {
            return self->mail(std::move(str)).delegate(delegatee);
          },
        };
      });
      SECTION("regular message") {
        self->mail("hello world")
          .request(delegator, infinite)
          .then([](const std::string&) {});
        auto self_hdl = actor_cast<actor>(self);
        launch();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self_hdl)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self_hdl)
          .to(delegatee);
        expect<std::string>()
          .with("dlrow olleh")
          .priority(message_priority::normal)
          .from(delegatee)
          .to(self_hdl);
      }
      SECTION("urgent message") {
        self->mail("hello world")
          .urgent()
          .request(delegator, infinite)
          .then([](const std::string&) {});
        auto self_hdl = actor_cast<actor>(self);
        launch();
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self_hdl)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self_hdl)
          .to(delegatee);
        expect<std::string>()
          .with("dlrow olleh")
          .priority(message_priority::high)
          .from(delegatee)
          .to(self_hdl);
      }
    }
    SECTION("delegate with high priority") {
      auto delegatee = sys.spawn([](event_based_actor*) -> behavior {
        return {
          [=](const std::string&) {},
        };
      });
      auto delegator = sys.spawn([delegatee](event_based_actor* self) {
        return behavior{
          [=](std::string& str) {
            return self->mail(std::move(str)).urgent().delegate(delegatee);
          },
        };
      });
      SECTION("regular message") {
        self->mail("hello world").send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::normal)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
      SECTION("urgent message") {
        self->mail("hello world").urgent().send(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegator);
        expect<std::string>()
          .with("hello world")
          .priority(message_priority::high)
          .from(self)
          .to(delegatee);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)
