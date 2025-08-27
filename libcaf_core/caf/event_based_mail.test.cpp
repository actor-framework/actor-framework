// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/event_based_mail.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/disposable.hpp"
#include "caf/dynamically_typed.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/log/test.hpp"
#include "caf/policy/select_all.hpp"
#include "caf/result.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/timespan.hpp"
#include "caf/type_id_list.hpp"
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

template <typename Fn>
caf::actor make_server(caf::actor_system& sys, Fn fn) {
  auto sf = [&]() -> behavior {
    return {
      [&](int x, int y) { return fn(x, y); },
    };
  };
  return sys.spawn(sf);
}

TEST("send fan_out_request messages that return a result") {
  std::vector<actor> workers{
    make_server(sys, [](int x, int y) { return x + y; }),
    make_server(sys, [](int x, int y) { return x + y; }),
    make_server(sys, [](int x, int y) { return x + y; }),
  };
  dispatch_messages();
  auto sum = std::make_shared<int>(0);
  auto err = std::make_shared<error>();
  SECTION("then with policy select_all") {
    auto sender = sys.spawn([workers, sum](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .then([=](std::vector<int> results) {
          for (auto result : results)
            test::runnable::current().check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int>().with(3).from(workers[0]).to(sender);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int>().with(3).from(workers[1]).to(sender);
    expect<int>().with(3).from(workers[2]).to(sender);
    check_eq(*sum, 9);
  }
  SECTION("then with policy select_any") {
    auto sender = sys.spawn([workers, sum](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .then([=](int result) { *sum = result; });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int>().with(3).from(workers[0]).to(sender);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int>().with(3).from(workers[1]).to(sender);
    expect<int>().with(3).from(workers[2]).to(sender);
    check_eq(*sum, 3);
  }
  SECTION("await with policy select_all") {
    auto sender = sys.spawn([this, workers, sum](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .await([this, sum](std::vector<int> results) {
          for (auto result : results)
            check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int>().with(3).from(workers[2]).to(sender);
    check_eq(mail_count(), 2u);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int>().with(3).from(workers[1]).to(sender);
    check_eq(mail_count(), 1u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int>().with(3).from(workers[0]).to(sender);
    check_eq(mail_count(), 0u);
    check_eq(*sum, 9);
  }
  SECTION("await with policy select_any") {
    auto sender = sys.spawn([workers, sum](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .await([sum](int result) { *sum = result; });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int>().with(3).from(workers[2]).to(sender);
    check_eq(*sum, 3);
    check_eq(mail_count(), 2u);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int>().with(3).from(workers[1]).to(sender);
    check_eq(mail_count(), 1u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int>().with(3).from(workers[0]).to(sender);
    check_eq(mail_count(), 0u);
    check_eq(*sum, 3);
  }
  SECTION(".to_observable with policy select_all") {
    auto sender = sys.spawn([workers, sum, err](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .as_observable<int>()
        .do_on_error([err](const error& x) { *err = x; })
        .for_each([sum](std::vector<int> results) {
          *sum = std::accumulate(results.begin(), results.end(), 0);
        });
    });
    dispatch_messages();
    check_eq(*err, error{});
    check_eq(*sum, 9);
  }
  SECTION(".to_observable with policy select_any") {
    auto sender = sys.spawn([workers, sum, err](event_based_actor* self) {
      self->mail(3, 5)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .as_observable<int>()
        .do_on_error([err](const error& x) { *err = x; })
        .for_each([sum](int x) { *sum = x; });
    });
    dispatch_messages();
    check_eq(*err, error{});
    check_eq(*sum, 8);
  }
  SECTION("error response") {
    auto sender = sys.spawn([workers, sum, err](event_based_actor* self) {
      self->mail("Hello")
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .as_observable<int>()
        .do_on_error([err](const error& x) { *err = x; })
        .for_each([sum](int x) { *sum = x; });
    });
    dispatch_messages();
    check_eq(*err, error{sec::all_requests_failed});
    check_eq(*sum, 0);
  }
}

TEST("send fan_out_request messages with void result") {
  std::vector<actor> workers{
    make_server(sys, [](int, int) {}),
    make_server(sys, [](int, int) {}),
    make_server(sys, [](int, int) {}),
  };
  dispatch_messages();
  auto ran = std::make_shared<bool>(false);
  SECTION("then with policy select_all") {
    auto sender = sys.spawn([workers, ran](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .then([ran]() { *ran = true; });
    });
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    dispatch_messages();
    check(*ran);
  }
  SECTION("then with policy select_any") {
    auto sender = sys.spawn([workers, ran](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .then([ran]() { *ran = true; });
    });
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    dispatch_messages();
    check(*ran);
  }
  SECTION("await with policy select_all") {
    auto sender = sys.spawn([workers, ran](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .await([ran]() { *ran = true; });
    });
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    dispatch_messages();
    check(*ran);
  }
  SECTION("await with policy select_any") {
    auto sender = sys.spawn([workers, ran](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .await([ran]() { *ran = true; });
    });
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    dispatch_messages();
    check(*ran);
  }
}

TEST("send fan_out_request messages that return two swapped values") {
  std::vector<actor> workers{
    make_server(sys, [](int x, int y) { return make_message(y, x); }),
    make_server(sys, [](int x, int y) { return make_message(y, x); }),
    make_server(sys, [](int x, int y) { return make_message(y, x); }),
  };
  dispatch_messages();
  auto swapped_values = std::make_shared<std::vector<std::pair<int, int>>>();
  auto single_result = std::make_shared<std::pair<int, int>>();
  auto err = std::make_shared<error>();
  SECTION("then with policy select_all") {
    auto sender = sys.spawn([workers, swapped_values](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .then([=](std::vector<std::tuple<int, int>> results) {
          for (auto result : results) {
            swapped_values->emplace_back(std::get<0>(result),
                                         std::get<1>(result));
          }
        });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int, int>().with(2, 1).from(workers[0]).to(sender);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int, int>().with(2, 1).from(workers[1]).to(sender);
    expect<int, int>().with(2, 1).from(workers[2]).to(sender);
    check_eq(swapped_values->size(), 3u);
    for (const auto& pair : *swapped_values) {
      check_eq(pair.first, 2);
      check_eq(pair.second, 1);
    }
  }
  SECTION("then with policy select_any") {
    auto sender = sys.spawn([workers, single_result](event_based_actor* self) {
      self->mail(3, 5)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .then([=](int first, int second) {
          *single_result = std::make_pair(first, second);
        });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(3, 5).from(sender).to(workers[0]);
    expect<int, int>().with(5, 3).from(workers[0]).to(sender);
    check_eq(single_result->first, 5);
    check_eq(single_result->second, 3);
    expect<int, int>().with(3, 5).from(sender).to(workers[1]);
    expect<int, int>().with(3, 5).from(sender).to(workers[2]);
    expect<int, int>().with(5, 3).from(workers[1]).to(sender);
    expect<int, int>().with(5, 3).from(workers[2]).to(sender);
  }
  SECTION("await with policy select_all") {
    auto sender
      = sys.spawn([this, workers, swapped_values](event_based_actor* self) {
          self->mail(7, 11)
            .fan_out_request(workers, infinite, policy::select_all_tag)
            .await([this,
                    swapped_values](std::vector<std::tuple<int, int>> results) {
              for (auto result : results) {
                check_eq(std::get<0>(result), 11);
                check_eq(std::get<1>(result), 7);
                swapped_values->emplace_back(std::get<0>(result),
                                             std::get<1>(result));
              }
            });
        });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(7, 11).from(sender).to(workers[2]);
    expect<int, int>().with(11, 7).from(workers[2]).to(sender);
    check_eq(mail_count(), 2u);
    expect<int, int>().with(7, 11).from(sender).to(workers[1]);
    expect<int, int>().with(11, 7).from(workers[1]).to(sender);
    check_eq(mail_count(), 1u);
    expect<int, int>().with(7, 11).from(sender).to(workers[0]);
    expect<int, int>().with(11, 7).from(workers[0]).to(sender);
    check_eq(mail_count(), 0u);
    check_eq(swapped_values->size(), 3u);
    for (const auto& pair : *swapped_values) {
      check_eq(pair.first, 11);
      check_eq(pair.second, 7);
    }
  }
  SECTION("await with policy select_any") {
    auto sender = sys.spawn([workers, single_result](event_based_actor* self) {
      self->mail(13, 17)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .await([single_result](int first, int second) {
          *single_result = std::make_pair(first, second);
        });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(13, 17).from(sender).to(workers[2]);
    expect<int, int>().with(17, 13).from(workers[2]).to(sender);
    check_eq(single_result->first, 17);
    check_eq(single_result->second, 13);
    check_eq(mail_count(), 2u);
    expect<int, int>().with(13, 17).from(sender).to(workers[1]);
    expect<int, int>().with(17, 13).from(workers[1]).to(sender);
    check_eq(mail_count(), 1u);
    expect<int, int>().with(13, 17).from(sender).to(workers[0]);
    expect<int, int>().with(17, 13).from(workers[0]).to(sender);
    check_eq(mail_count(), 0u);
    check_eq(single_result->first, 17);
    check_eq(single_result->second, 13);
  }
  SECTION("as_observable with policy select_all") {
    auto sender
      = sys.spawn([workers, swapped_values, err](event_based_actor* self) {
          self->mail(19, 23)
            .fan_out_request(workers, infinite, policy::select_all_tag)
            .as_observable<int, int>()
            .do_on_error([err](const error& x) { *err = x; })
            .for_each(
              [swapped_values](std::vector<caf::cow_tuple<int, int>> results) {
                swapped_values->clear();
                for (auto result : results) {
                  swapped_values->emplace_back(get<0>(result), get<1>(result));
                }
              });
        });
    dispatch_messages();
    check_eq(*err, error{});
    check_eq(swapped_values->size(), 3u);
    for (const auto& pair : *swapped_values) {
      check_eq(pair.first, 23);
      check_eq(pair.second, 19);
    }
  }
  SECTION("as_observable with policy select_any") {
    auto sender
      = sys.spawn([workers, single_result, err](event_based_actor* self) {
          self->mail(29, 31)
            .fan_out_request(workers, infinite, policy::select_any_tag)
            .as_observable<int, int>()
            .do_on_error([err](const error& x) { *err = x; })
            .for_each([single_result](caf::cow_tuple<int, int> result) {
              *single_result = std::make_pair(get<0>(result), get<1>(result));
            });
        });
    dispatch_messages();
    check_eq(*err, error{});
    check_eq(single_result->first, 31);
    check_eq(single_result->second, 29);
  }
  SECTION("error response with swapped values") {
    auto error_workers = std::vector<actor>{
      make_server(sys, [](int, int) { return "error"s; }),
      make_server(sys, [](int, int) { return "error"s; }),
      make_server(sys, [](int, int) { return "error"s; }),
    };
    dispatch_messages();
    auto sender
      = sys.spawn([error_workers, single_result, err](event_based_actor* self) {
          self->mail(37, 41)
            .fan_out_request(error_workers, infinite, policy::select_any_tag)
            .as_observable<int, int>()
            .do_on_error([err](const error& x) { *err = x; })
            .for_each([single_result](cow_tuple<int, int> result) {
              *single_result = std::make_pair(get<0>(result), get<0>(result));
            });
        });
    dispatch_messages();
    check_eq(*err, error{sec::all_requests_failed});
    check_eq(single_result->first, 0);
    check_eq(single_result->second, 0);
  }
}

// Typed actor interfaces for the fan_out_request test.
using typed_worker_actor = typed_actor<result<int>(int, int)>;
using typed_worker_behavior = typed_worker_actor::behavior_type;
using typed_worker_two_values_actor = typed_actor<result<int, int>(int, int)>;
using typed_worker_two_values_behavior
  = typed_worker_two_values_actor::behavior_type;
using typed_worker_void_actor = typed_actor<result<void>(int, int)>;
using typed_worker_void_behavior = typed_worker_void_actor::behavior_type;

template <typename Fn>
typed_worker_actor make_typed_server(caf::actor_system& sys, Fn fn) {
  auto sf = [&]() -> typed_worker_behavior {
    return {[&](int x, int y) { return fn(x, y); }};
  };
  return sys.spawn(sf);
}

template <typename Fn>
typed_worker_two_values_actor
make_typed_server_two_values(caf::actor_system& sys, Fn fn) {
  auto sf = [&]() -> typed_worker_two_values_behavior {
    return {[&](int x, int y) -> result<int, int> { return fn(x, y); }};
  };
  return sys.spawn(sf);
}

template <typename Fn>
typed_worker_void_actor make_typed_server_void(caf::actor_system& sys, Fn fn) {
  auto sf = [&]() -> typed_worker_void_behavior {
    return {[&](int x, int y) -> result<void> {
      fn(x, y);
      return {};
    }};
  };
  return sys.spawn(sf);
}

TEST("send fan_out_request messages that return a result using typed actors") {
  std::vector<typed_worker_actor> workers{
    make_typed_server(sys, [](int x, int y) { return x + y; }),
    make_typed_server(sys, [](int x, int y) { return x + y; }),
    make_typed_server(sys, [](int x, int y) { return x + y; }),
  };
  dispatch_messages();
  auto sum = std::make_shared<int>(0);
  auto err = std::make_shared<error>();
  SECTION("then with policy select_all") {
    auto sender = sys.spawn([workers, sum, err](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .then([=](std::vector<int> results) {
          for (auto result : results)
            test::runnable::current().check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int>().with(3).from(workers[0]).to(sender);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int>().with(3).from(workers[1]).to(sender);
    expect<int>().with(3).from(workers[2]).to(sender);
    check_eq(*sum, 9);
  }
  SECTION("then with policy select_any") {
    auto sender = sys.spawn([workers, sum, err](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .then([=](int result) { *sum = result; });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int>().with(3).from(workers[0]).to(sender);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int>().with(3).from(workers[1]).to(sender);
    expect<int>().with(3).from(workers[2]).to(sender);
    check_eq(*sum, 3);
  }
  SECTION("await with policy select_all") {
    auto sender = sys.spawn([this, workers, sum](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .await([this, sum](std::vector<int> results) {
          for (auto result : results)
            check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int>().with(3).from(workers[2]).to(sender);
    check_eq(mail_count(), 2u);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int>().with(3).from(workers[1]).to(sender);
    check_eq(mail_count(), 1u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int>().with(3).from(workers[0]).to(sender);
    check_eq(mail_count(), 0u);
    check_eq(*sum, 9);
  }
  SECTION("await with policy select_any") {
    auto sender = sys.spawn([workers, sum](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .await([sum](int result) { *sum = result; });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int>().with(3).from(workers[2]).to(sender);
    check_eq(*sum, 3);
    check_eq(mail_count(), 2u);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int>().with(3).from(workers[1]).to(sender);
    check_eq(mail_count(), 1u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int>().with(3).from(workers[0]).to(sender);
    check_eq(mail_count(), 0u);
    check_eq(*sum, 3);
  }
  SECTION(".to_observable with policy select_all") {
    auto sender = sys.spawn([workers, sum, err](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .as_observable()
        .do_on_error([err](const error& x) { *err = x; })
        .for_each([sum](std::vector<int> results) {
          *sum = std::accumulate(results.begin(), results.end(), 0);
        });
    });
    dispatch_messages();
    check_eq(*err, error{});
    check_eq(*sum, 9);
  }
  SECTION(".to_observable with policy select_any") {
    auto sender = sys.spawn([workers, sum, err](event_based_actor* self) {
      self->mail(3, 5)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .as_observable()
        .do_on_error([err](const error& x) { *err = x; })
        .for_each([sum](int x) { *sum = x; });
    });
    dispatch_messages();
    check_eq(*err, error{});
    check_eq(*sum, 8);
  }
  SECTION("error response") {
    std::vector<typed_worker_actor> error_workers{
      make_typed_server(sys,
                        [](int, int) -> result<int> {
                          return caf::error(sec::logic_error);
                        }),
      make_typed_server(sys,
                        [](int, int) -> result<int> {
                          return caf::error(sec::logic_error);
                        }),
      make_typed_server(sys,
                        [](int, int) -> result<int> {
                          return caf::error(sec::logic_error);
                        }),
    };
    auto sender = sys.spawn([error_workers, sum, err](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(error_workers, infinite, policy::select_any_tag)
        .then([sum](int result) { *sum = result; },
              [err](error& e) { *err = std::move(e); });
    });
    dispatch_messages();
    check_eq(*err, error{sec::all_requests_failed});
    check_eq(*sum, 0);
  }
}

TEST("send fan_out_request messages that return two swapped values using typed "
     "actors") {
  std::vector<typed_worker_two_values_actor> workers{
    make_typed_server_two_values(
      sys, [](int x, int y) { return result<int, int>(y, x); }),
    make_typed_server_two_values(
      sys, [](int x, int y) { return result<int, int>(y, x); }),
    make_typed_server_two_values(
      sys, [](int x, int y) { return result<int, int>(y, x); }),
  };
  dispatch_messages();
  auto swapped_values = std::make_shared<std::vector<std::pair<int, int>>>();
  auto single_result = std::make_shared<std::pair<int, int>>();
  auto err = std::make_shared<error>();
  SECTION("then with policy select_all") {
    auto sender = sys.spawn(
      [workers, swapped_values, single_result, err](event_based_actor* self) {
        self->mail(1, 2)
          .fan_out_request(workers, infinite, policy::select_all_tag)
          .then([=](std::vector<std::tuple<int, int>> results) {
            for (auto result : results) {
              swapped_values->emplace_back(std::get<0>(result),
                                           std::get<1>(result));
            }
          });
      });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int, int>().with(2, 1).from(workers[0]).to(sender);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int, int>().with(2, 1).from(workers[1]).to(sender);
    expect<int, int>().with(2, 1).from(workers[2]).to(sender);
    check_eq(swapped_values->size(), 3u);
    for (const auto& pair : *swapped_values) {
      check_eq(pair.first, 2);
      check_eq(pair.second, 1);
    }
  }
  SECTION("then with policy select_any") {
    auto sender = sys.spawn(
      [workers, swapped_values, single_result, err](event_based_actor* self) {
        self->mail(3, 5)
          .fan_out_request(workers, infinite, policy::select_any_tag)
          .then([=](int first, int second) {
            *single_result = std::make_pair(first, second);
          });
      });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(3, 5).from(sender).to(workers[0]);
    expect<int, int>().with(5, 3).from(workers[0]).to(sender);
    check_eq(single_result->first, 5);
    check_eq(single_result->second, 3);
    expect<int, int>().with(3, 5).from(sender).to(workers[1]);
    expect<int, int>().with(3, 5).from(sender).to(workers[2]);
    expect<int, int>().with(5, 3).from(workers[1]).to(sender);
    expect<int, int>().with(5, 3).from(workers[2]).to(sender);
  }
  SECTION("await with policy select_all") {
    auto sender
      = sys.spawn([this, workers, swapped_values](event_based_actor* self) {
          self->mail(7, 11)
            .fan_out_request(workers, infinite, policy::select_all_tag)
            .await([this,
                    swapped_values](std::vector<std::tuple<int, int>> results) {
              for (auto result : results) {
                check_eq(std::get<0>(result), 11);
                check_eq(std::get<1>(result), 7);
                swapped_values->emplace_back(std::get<0>(result),
                                             std::get<1>(result));
              }
            });
        });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(7, 11).from(sender).to(workers[2]);
    expect<int, int>().with(11, 7).from(workers[2]).to(sender);
    check_eq(mail_count(), 2u);
    expect<int, int>().with(7, 11).from(sender).to(workers[1]);
    expect<int, int>().with(11, 7).from(workers[1]).to(sender);
    check_eq(mail_count(), 1u);
    expect<int, int>().with(7, 11).from(sender).to(workers[0]);
    expect<int, int>().with(11, 7).from(workers[0]).to(sender);
    check_eq(mail_count(), 0u);
    check_eq(swapped_values->size(), 3u);
    for (const auto& pair : *swapped_values) {
      check_eq(pair.first, 11);
      check_eq(pair.second, 7);
    }
  }
  SECTION("await with policy select_any") {
    auto sender = sys.spawn([workers, single_result](event_based_actor* self) {
      self->mail(13, 17)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .await([single_result](int first, int second) {
          *single_result = std::make_pair(first, second);
        });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(13, 17).from(sender).to(workers[2]);
    expect<int, int>().with(17, 13).from(workers[2]).to(sender);
    check_eq(single_result->first, 17);
    check_eq(single_result->second, 13);
    check_eq(mail_count(), 2u);
    expect<int, int>().with(13, 17).from(sender).to(workers[1]);
    expect<int, int>().with(17, 13).from(workers[1]).to(sender);
    check_eq(mail_count(), 1u);
    expect<int, int>().with(13, 17).from(sender).to(workers[0]);
    expect<int, int>().with(17, 13).from(workers[0]).to(sender);
    check_eq(mail_count(), 0u);
    check_eq(single_result->first, 17);
    check_eq(single_result->second, 13);
  }
  SECTION("as_observable with policy select_all") {
    auto sender
      = sys.spawn([workers, swapped_values, err](event_based_actor* self) {
          self->mail(19, 23)
            .fan_out_request(workers, infinite, policy::select_all_tag)
            .as_observable()
            .do_on_error([err](const error& x) { *err = x; })
            .for_each(
              [swapped_values](std::vector<caf::cow_tuple<int, int>> results) {
                swapped_values->clear();
                for (auto result : results) {
                  swapped_values->emplace_back(get<0>(result), get<1>(result));
                }
              });
        });
    dispatch_messages();
    check_eq(*err, error{});
    check_eq(swapped_values->size(), 3u);
    for (const auto& pair : *swapped_values) {
      check_eq(pair.first, 23);
      check_eq(pair.second, 19);
    }
  }
  SECTION("as_observable with policy select_any") {
    auto sender
      = sys.spawn([workers, single_result, err](event_based_actor* self) {
          self->mail(29, 31)
            .fan_out_request(workers, infinite, policy::select_any_tag)
            .as_observable()
            .do_on_error([err](const error& x) { *err = x; })
            .for_each([single_result](caf::cow_tuple<int, int> result) {
              *single_result = std::make_pair(get<0>(result), get<1>(result));
            });
        });
    dispatch_messages();
    check_eq(*err, error{});
    check_eq(single_result->first, 31);
    check_eq(single_result->second, 29);
  }
  SECTION("error response with swapped values") {
    std::vector<typed_worker_two_values_actor> error_workers{
      make_typed_server_two_values(sys,
                                   [](int, int) -> result<int, int> {
                                     return caf::error(sec::logic_error);
                                   }),
      make_typed_server_two_values(sys,
                                   [](int, int) -> result<int, int> {
                                     return caf::error(sec::logic_error);
                                   }),
      make_typed_server_two_values(sys,
                                   [](int, int) -> result<int, int> {
                                     return caf::error(sec::logic_error);
                                   }),
    };
    auto sender
      = sys.spawn([error_workers, single_result, err](event_based_actor* self) {
          self->mail(37, 41)
            .fan_out_request(error_workers, infinite, policy::select_any_tag)
            .as_observable()
            .do_on_error([err](const error& e) { *err = e; })
            .for_each([single_result](cow_tuple<int, int> result) {
              *single_result = std::make_pair(get<0>(result), get<1>(result));
            });
        });
    dispatch_messages();
    check_eq(*err, error{sec::all_requests_failed});
    check_eq(single_result->first, 0);
    check_eq(single_result->second, 0);
  }
}

TEST("send fan_out_request messages with void result using typed actors") {
  std::vector<typed_worker_void_actor> workers{
    make_typed_server_void(sys, [](int, int) {}),
    make_typed_server_void(sys, [](int, int) {}),
    make_typed_server_void(sys, [](int, int) {}),
  };
  dispatch_messages();
  auto ran = std::make_shared<bool>(false);
  SECTION("then with policy select_all") {
    auto sender = sys.spawn([workers, ran](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .then([ran]() { *ran = true; });
    });
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    dispatch_messages();
    check(*ran);
  }
  SECTION("then with policy select_any") {
    auto sender = sys.spawn([workers, ran](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .then([ran]() { *ran = true; });
    });
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    dispatch_messages();
    check(*ran);
  }
  SECTION("await with policy select_all") {
    auto sender = sys.spawn([workers, ran](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .await([ran]() { *ran = true; });
    });
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    dispatch_messages();
    check(*ran);
  }
  SECTION("await with policy select_any") {
    auto sender = sys.spawn([workers, ran](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .await([ran]() { *ran = true; });
    });
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    dispatch_messages();
    check(*ran);
  }
}

TEST("send fan_out_request messages with invalid setups") {
  std::vector<actor> workers{
    make_server(sys, [](int x, int y) { return std::to_string(x + y); }),
    make_server(sys, [](int x, int y) { return std::to_string(x + y); }),
    make_server(sys, [](int x, int y) { return std::to_string(x + y); }),
  };
  dispatch_messages();
  auto err = std::make_shared<error>();
  SECTION("then with policy select_all") {
    auto sender = sys.spawn([workers, err](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .then(
          [=](std::vector<int> results) {
            test::runnable::current().fail("expected an error, got: {}",
                                           results);
          },
          [=](const error& e) { *err = std::move(e); });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<std::string>().with("3").from(workers[0]).to(sender);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<std::string>().with("3").from(workers[1]).to(sender);
    expect<std::string>().with("3").from(workers[2]).to(sender);
    check_eq(*err, make_error(sec::unexpected_response));
  }
  SECTION("then with policy select_any") {
    auto sender = sys.spawn([workers, err](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .then(
          [=](int results) {
            test::runnable::current().fail("expected an error, got: {}",
                                           results);
          },
          [=](const error& e) { *err = std::move(e); });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<std::string>().with("3").from(workers[0]).to(sender);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<std::string>().with("3").from(workers[1]).to(sender);
    expect<std::string>().with("3").from(workers[2]).to(sender);
    check_eq(*err, make_error(sec::all_requests_failed));
  }
  SECTION("await with policy select_all") {
    auto sender = sys.spawn([workers, err](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_all_tag)
        .await(
          [=](std::vector<int> results) {
            test::runnable::current().fail("expected an error, got: {}",
                                           results);
          },
          [=](const error& e) { *err = std::move(e); });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<std::string>().with("3").from(workers[2]).to(sender);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<std::string>().with("3").from(workers[1]).to(sender);
    expect<std::string>().with("3").from(workers[0]).to(sender);
    check_eq(*err, make_error(sec::unexpected_response));
  }
  SECTION("await with policy select_any") {
    auto sender = sys.spawn([workers, err](event_based_actor* self) {
      self->mail(1, 2)
        .fan_out_request(workers, infinite, policy::select_any_tag)
        .then(
          [=](int results) {
            test::runnable::current().fail("expected an error, got: {}",
                                           results);
          },
          [=](const error& e) { *err = std::move(e); });
    });
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(sender).to(workers[2]);
    expect<std::string>().with("3").from(workers[2]).to(sender);
    expect<int, int>().with(1, 2).from(sender).to(workers[1]);
    expect<int, int>().with(1, 2).from(sender).to(workers[0]);
    expect<std::string>().with("3").from(workers[1]).to(sender);
    expect<std::string>().with("3").from(workers[0]).to(sender);
    check_eq(*err, make_error(sec::all_requests_failed));
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

TEST("send delayed fan_out_request messages that return a result") {
  auto [self, launch] = sys.spawn_inactive();
  auto self_hdl = actor_cast<actor>(self);
  std::vector<actor> workers{
    make_server(sys, [](int x, int y) { return x + y; }),
    make_server(sys, [](int x, int y) { return x + y; }),
    make_server(sys, [](int x, int y) { return x + y; }),
  };
  dispatch_messages();
  auto sum = std::make_shared<int>(0);
  auto err = std::make_shared<error>();
  SECTION("then with policy select_all") {
    self->mail(1, 2)
      .delay(1s)
      .fan_out_request(workers, infinite, policy::select_all_tag)
      .then([=](std::vector<int> results) {
        for (auto result : results)
          test::runnable::current().check_eq(result, 3);
        *sum = std::accumulate(results.begin(), results.end(), 0);
      });
    launch();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 3u);
    trigger_all_timeouts();
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[0]);
    expect<int>().with(3).from(workers[0]).to(self_hdl);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[1]);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[2]);
    expect<int>().with(3).from(workers[1]).to(self_hdl);
    expect<int>().with(3).from(workers[2]).to(self_hdl);
    check_eq(*sum, 9);
  }
  SECTION("then with policy select_any") {
    self->mail(1, 2)
      .delay(1s)
      .fan_out_request(workers, infinite, policy::select_any_tag)
      .then([=](int result) { *sum = result; });
    launch();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 3u);
    trigger_all_timeouts();
    check_eq(mail_count(), 3u);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[0]);
    expect<int>().with(3).from(workers[0]).to(self_hdl);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[1]);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[2]);
    expect<int>().with(3).from(workers[1]).to(self_hdl);
    expect<int>().with(3).from(workers[2]).to(self_hdl);
    check_eq(*sum, 3);
  }
}

TEST("timeout a fan_out_request") {
  auto [self, launch] = sys.spawn_inactive();
  auto self_hdl = actor_cast<actor>(self);
  auto empty_promises = std::make_shared<std::vector<response_promise>>();
  auto dummy = [self, empty_promises](int, int) {
    empty_promises->push_back(self->make_response_promise());
    return empty_promises->back();
  };
  std::vector<actor> workers{
    make_server(sys, dummy),
    make_server(sys, dummy),
    make_server(sys, dummy),
  };
  dispatch_messages();
  auto sum = std::make_shared<int>(0);
  auto err = std::make_shared<error>();
  SECTION("an immediate request") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .then(
        [=](std::vector<int> results) {
          for (auto result : results)
            test::runnable::current().check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        },
        [err](error& e) { *err = std::move(e); });
    launch();
    check_eq(mail_count(), 3u);
    check_eq(num_timeouts(), 3u);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[0]);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[1]);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[2]);
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 3u);
    advance_time(1s);
    check_eq(num_timeouts(), 0u);
    auto timeout_error = error(sec::request_timeout);
    expect<error>().with(timeout_error).to(self_hdl);
    expect<error>().with(timeout_error).to(self_hdl);
    expect<error>().with(timeout_error).to(self_hdl);
    check_eq(*err, make_error(sec::request_timeout));
  }
  SECTION("a delayed request") {
    self->mail(1, 2)
      .delay(1s)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .then(
        [=](std::vector<int> results) {
          for (auto result : results)
            test::runnable::current().check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        },
        [err](error& e) { *err = std::move(e); });
    launch();
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 6u);
    advance_time(1s);
    check_eq(mail_count(), 3u);
    check_eq(num_timeouts(), 3u);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[0]);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[1]);
    expect<int, int>().with(1, 2).from(self_hdl).to(workers[2]);
    check_eq(mail_count(), 0u);
    check_eq(num_timeouts(), 3u);
    advance_time(1s);
    check_eq(num_timeouts(), 0u);
    auto timeout_error = error(sec::request_timeout);
    expect<error>().with(timeout_error).to(self_hdl);
    expect<error>().with(timeout_error).to(self_hdl);
    expect<error>().with(timeout_error).to(self_hdl);
    check_eq(*err, make_error(sec::request_timeout));
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)
