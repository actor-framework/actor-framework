// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/blocking_actor.hpp"
#include "caf/blocking_mail.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

#include <iostream>
#include <numeric>

using namespace caf;
using namespace std::literals;

namespace {

struct config : actor_system_config {
  config() {
    set("caf.scheduler.max-threads", 2u);
  }
};

struct fixture {
  config cfg;
  actor_system sys{cfg};
};

template <typename Fn>
actor make_server(caf::actor_system& sys, Fn fn) {
  auto sf = [fn]() -> behavior {
    return {
      [fn](int x, int y) { return fn(x, y); },
    };
  };
  return sys.spawn(sf);
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
  auto sf = [fn]() -> typed_worker_behavior {
    return {[fn](int x, int y) { return fn(x, y); }};
  };
  return sys.spawn(sf);
}

template <typename Fn>
typed_worker_two_values_actor
make_typed_server_two_values(caf::actor_system& sys, Fn fn) {
  auto sf = [fn]() -> typed_worker_two_values_behavior {
    return {[fn](int x, int y) -> result<int, int> { return fn(x, y); }};
  };
  return sys.spawn(sf);
}

template <typename Fn>
typed_worker_void_actor make_typed_server_void(caf::actor_system& sys, Fn fn) {
  auto sf = [fn]() -> typed_worker_void_behavior {
    return {[fn](int x, int y) -> result<void> {
      fn(x, y);
      return {};
    }};
  };
  return sys.spawn(sf);
}

} // namespace

WITH_FIXTURE(fixture) {

TEST("fan_out_request with a single result") {
  scoped_actor self{sys};
  std::vector<actor> workers{
    make_server(sys, [](int x, int y) { return x + y; }),
    make_server(sys, [](int x, int y) { return x + y; }),
    make_server(sys, [](int x, int y) { return x + y; }),
  };
  auto sum = std::make_shared<int>(0);
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive(
        [this, sum](std::vector<int> results) {
          for (auto result : results)
            check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(*sum, 9);
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_all") {
    auto res = self->mail(1, 2)
                 .fan_out_request(workers, 1s, policy::select_all_tag)
                 .receive<int>();
    if (check_has_value(res)) {
      check_eq(res->size(), 3u);
      for (const auto& val : *res) {
        check_eq(val, 3);
      }
    }
  }
  SECTION("receive with policy select_any") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive([sum](int result) { *sum = result; },
               [err](error& e) { *err = std::move(e); });
    check_eq(*sum, 3);
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_any") {
    auto res = self->mail(1, 2)
                 .fan_out_request(workers, 1s, policy::select_any_tag)
                 .receive<int>();
    check_eq(res, 3);
  }
}

TEST("fan_out_request with void result") {
  scoped_actor self{sys};
  std::vector<actor> workers{
    make_server(sys, [](int, int) {}),
    make_server(sys, [](int, int) {}),
    make_server(sys, [](int, int) {}),
  };
  auto ran = std::make_shared<bool>(false);
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive([ran]() { *ran = true; },
               [err](error& e) { *err = std::move(e); });
    check(*ran);
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_all") {
    auto res = self->mail(1, 2)
                 .fan_out_request(workers, 1s, policy::select_all_tag)
                 .receive();
    check_has_value(res);
  }
  SECTION("receive with policy select_any") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive([ran]() { *ran = true; },
               [err](error& e) { *err = std::move(e); });
    check(*ran);
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_any") {
    auto res = self->mail(1, 2)
                 .fan_out_request(workers, 1s, policy::select_any_tag)
                 .receive();
    check_has_value(res);
  }
}

TEST("fan_out_request with multiple results") {
  scoped_actor self{sys};
  std::vector<actor> workers{
    make_server(sys, [](int x, int y) { return make_message(y, x); }),
    make_server(sys, [](int x, int y) { return make_message(y, x); }),
    make_server(sys, [](int x, int y) { return make_message(y, x); }),
  };
  auto swapped_values = std::make_shared<std::vector<std::pair<int, int>>>();
  auto single_result = std::make_shared<std::pair<int, int>>();
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive(
        [swapped_values](std::vector<std::tuple<int, int>> results) {
          for (auto result : results) {
            swapped_values->emplace_back(std::get<0>(result),
                                         std::get<1>(result));
          }
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(swapped_values->size(), 3u);
    for (const auto& pair : *swapped_values) {
      check_eq(pair.first, 2);
      check_eq(pair.second, 1);
    }
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_all") {
    auto res = self->mail(1, 2)
                 .fan_out_request(workers, 1s, policy::select_all_tag)
                 .receive<int, int>();
    if (check_has_value(res)) {
      check_eq(res->size(), 3u);
      for (const auto& pair : *res) {
        check_eq(std::get<0>(pair), 2);
        check_eq(std::get<1>(pair), 1);
      }
    }
  }
  SECTION("receive with policy select_any") {
    self->mail(3, 5)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive(
        [single_result](int first, int second) {
          *single_result = std::make_pair(first, second);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(single_result->first, 5);
    check_eq(single_result->second, 3);
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_any") {
    auto res = self->mail(3, 5)
                 .fan_out_request(workers, 1s, policy::select_any_tag)
                 .receive<int, int>();
    if (check_has_value(res)) {
      check_eq(std::get<0>(*res), 5);
      check_eq(std::get<1>(*res), 3);
    }
  }
}

TEST("fan_out_request with type mismatch") {
  scoped_actor self{sys};
  std::vector<actor> workers{
    make_server(sys, [](int x, int y) { return std::to_string(x + y); }),
    make_server(sys, [](int x, int y) { return std::to_string(x + y); }),
    make_server(sys, [](int x, int y) { return std::to_string(x + y); }),
  };
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive(
        [this](std::vector<int> results) {
          fail("expected an error, got: {}", results);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(*err, make_error(sec::unexpected_response));
  }
  SECTION("receive with policy select_any") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive([this](
                 int results) { fail("expected an error, got: {}", results); },
               [err](error& e) { *err = std::move(e); });
    check_eq(*err, make_error(sec::unexpected_response));
  }
}

TEST("fan_out_request with timeout") {
  scoped_actor self{sys};
  auto dummy = [](event_based_actor* self) -> behavior {
    auto res = std::make_shared<response_promise>();
    return {
      [self, res](int, int) {
        *res = self->make_response_promise();
        return *res;
      },
    };
  };
  std::vector<actor> workers{sys.spawn(dummy), sys.spawn(dummy),
                             sys.spawn(dummy)};
  auto sum = std::make_shared<int>(0);
  auto err = std::make_shared<error>();
  SECTION("timeout with policy select_all") {
    self->mail(1, 2)
      .fan_out_request(workers, 10ms, policy::select_all_tag)
      .receive(
        [sum, this](std::vector<int> results) {
          for (auto result : results)
            check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(*err, make_error(sec::request_timeout));
  }
  SECTION("timeout with policy select_any") {
    self->mail(1, 2)
      .fan_out_request(workers, 10ms, policy::select_any_tag)
      .receive([sum](int result) { *sum = result; },
               [err](error& e) { *err = std::move(e); });
    check_eq(*err, make_error(sec::request_timeout));
  }
  for (const auto& worker : workers) {
    self->send_exit(worker, exit_reason::user_shutdown);
  }
}

TEST("typed fan_out_request with a single result") {
  scoped_actor self{sys};
  std::vector<typed_worker_actor> workers{
    make_typed_server(sys, [](int x, int y) { return x + y; }),
    make_typed_server(sys, [](int x, int y) { return x + y; }),
    make_typed_server(sys, [](int x, int y) { return x + y; }),
  };
  auto sum = std::make_shared<int>(0);
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive(
        [sum, this](std::vector<int> results) {
          for (auto result : results)
            check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(*sum, 9);
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_all") {
    auto res = self->mail(1, 2)
                 .fan_out_request(workers, 1s, policy::select_all_tag)
                 .receive();
    if (check_has_value(res)) {
      check_eq(res->size(), 3u);
      for (const auto& val : *res) {
        check_eq(val, 3);
      }
    }
  }
  SECTION("receive with policy select_any") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive([sum](int result) { *sum = result; },
               [err](error& e) { *err = std::move(e); });
    check_eq(*sum, 3);
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_any") {
    auto res = self->mail(1, 2)
                 .fan_out_request(workers, 1s, policy::select_any_tag)
                 .receive();
    check_eq(res, 3);
  }
}

TEST("typed fan_out_request with multiple results") {
  scoped_actor self{sys};
  std::vector<typed_worker_two_values_actor> workers{
    make_typed_server_two_values(
      sys, [](int x, int y) { return result<int, int>(y, x); }),
    make_typed_server_two_values(
      sys, [](int x, int y) { return result<int, int>(y, x); }),
    make_typed_server_two_values(
      sys, [](int x, int y) { return result<int, int>(y, x); }),
  };
  auto swapped_values = std::make_shared<std::vector<std::pair<int, int>>>();
  auto single_result = std::make_shared<std::pair<int, int>>();
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive(
        [swapped_values](std::vector<std::tuple<int, int>> results) {
          for (auto result : results) {
            swapped_values->emplace_back(std::get<0>(result),
                                         std::get<1>(result));
          }
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(swapped_values->size(), 3u);
    for (const auto& pair : *swapped_values) {
      check_eq(pair.first, 2);
      check_eq(pair.second, 1);
    }
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_all") {
    auto res = self->mail(1, 2)
                 .fan_out_request(workers, 1s, policy::select_all_tag)
                 .receive();
    if (check_has_value(res)) {
      check_eq(res->size(), 3u);
      for (const auto& pair : *res) {
        check_eq(std::get<0>(pair), 2);
        check_eq(std::get<1>(pair), 1);
      }
    }
  }
  SECTION("receive with policy select_any") {
    self->mail(3, 5)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive(
        [single_result](int first, int second) {
          *single_result = std::make_pair(first, second);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(single_result->first, 5);
    check_eq(single_result->second, 3);
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_any") {
    auto res = self->mail(3, 5)
                 .fan_out_request(workers, 1s, policy::select_any_tag)
                 .receive();
    if (check_has_value(res)) {
      check_eq(std::get<0>(*res), 5);
      check_eq(std::get<1>(*res), 3);
    }
  }
}

TEST("typed fan_out_request with void result") {
  scoped_actor self{sys};
  std::vector<typed_worker_void_actor> workers{
    make_typed_server_void(sys, [](int, int) {}),
    make_typed_server_void(sys, [](int, int) {}),
    make_typed_server_void(sys, [](int, int) {}),
  };
  auto ran = std::make_shared<bool>(false);
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive([ran]() { *ran = true; },
               [err](error& e) { *err = std::move(e); });
    check(*ran);
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_all") {
    auto res = self->mail(1, 2)
                 .fan_out_request(workers, 1s, policy::select_all_tag)
                 .receive();
    check_has_value(res);
  }
  SECTION("receive with policy select_any") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive([ran]() { *ran = true; },
               [err](error& e) { *err = std::move(e); });
    check(*ran);
    check_eq(*err, error{});
  }
  SECTION("receive to expected with policy select_any") {
    auto res = self->mail(1, 2)
                 .fan_out_request(workers, 1s, policy::select_any_tag)
                 .receive();
    check_has_value(res);
  }
}

TEST("typed fan_out_request with error responses") {
  scoped_actor self{sys};
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
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .fan_out_request(error_workers, 1s, policy::select_all_tag)
      .receive(
        [this](std::vector<int> results) {
          fail("expected an error, got: {}", results);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(*err, make_error(sec::logic_error));
  }
  SECTION("receive with policy select_any") {
    self->mail(1, 2)
      .fan_out_request(error_workers, 1s, policy::select_any_tag)
      .receive([this](
                 int results) { fail("expected an error, got: {}", results); },
               [err](error& e) { *err = std::move(e); });
    check_eq(*err, make_error(sec::logic_error));
  }
}

TEST("delayed fan_out_request with a single result") {
  scoped_actor self{sys};
  std::vector<actor> workers{
    make_server(sys, [](int x, int y) { return x + y; }),
    make_server(sys, [](int x, int y) { return x + y; }),
    make_server(sys, [](int x, int y) { return x + y; }),
  };
  auto sum = std::make_shared<int>(0);
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive(
        [sum, this](std::vector<int> results) {
          for (auto result : results)
            check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(*sum, 9);
    check_eq(*err, error{});
  }
  SECTION("receive with policy select_any") {
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive([sum](int result) { *sum = result; },
               [err](error& e) { *err = std::move(e); });
    check_eq(*sum, 3);
    check_eq(*err, error{});
  }
}

TEST("delayed fan_out_request with void result") {
  scoped_actor self{sys};
  std::vector<actor> workers{
    make_server(sys, [](int, int) {}),
    make_server(sys, [](int, int) {}),
    make_server(sys, [](int, int) {}),
  };
  auto ran = std::make_shared<bool>(false);
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive([ran]() { *ran = true; },
               [err](error& e) { *err = std::move(e); });
    check(*ran);
    check_eq(*err, error{});
  }
  SECTION("receive with policy select_any") {
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive([ran]() { *ran = true; },
               [err](error& e) { *err = std::move(e); });
    check(*ran);
    check_eq(*err, error{});
  }
}

TEST("delayed fan_out_request with multiple results") {
  scoped_actor self{sys};
  std::vector<actor> workers{
    make_server(sys, [](int x, int y) { return make_message(y, x); }),
    make_server(sys, [](int x, int y) { return make_message(y, x); }),
    make_server(sys, [](int x, int y) { return make_message(y, x); }),
  };
  auto swapped_values = std::make_shared<std::vector<std::pair<int, int>>>();
  auto single_result = std::make_shared<std::pair<int, int>>();
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive(
        [swapped_values](std::vector<std::tuple<int, int>> results) {
          for (auto result : results) {
            swapped_values->emplace_back(std::get<0>(result),
                                         std::get<1>(result));
          }
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(swapped_values->size(), 3u);
    for (const auto& pair : *swapped_values) {
      check_eq(pair.first, 2);
      check_eq(pair.second, 1);
    }
    check_eq(*err, error{});
  }
  SECTION("receive with policy select_any") {
    self->mail(3, 5)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive(
        [single_result](int first, int second) {
          *single_result = std::make_pair(first, second);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(single_result->first, 5);
    check_eq(single_result->second, 3);
    check_eq(*err, error{});
  }
}

TEST("typed delayed fan_out_request with a single result") {
  scoped_actor self{sys};
  std::vector<typed_worker_actor> workers{
    make_typed_server(sys, [](int x, int y) { return x + y; }),
    make_typed_server(sys, [](int x, int y) { return x + y; }),
    make_typed_server(sys, [](int x, int y) { return x + y; }),
  };
  auto sum = std::make_shared<int>(0);
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive(
        [sum, this](std::vector<int> results) {
          for (auto result : results)
            check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(*sum, 9);
    check_eq(*err, error{});
  }
  SECTION("receive with policy select_any") {
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive([sum](int result) { *sum = result; },
               [err](error& e) { *err = std::move(e); });
    check_eq(*sum, 3);
    check_eq(*err, error{});
  }
}

TEST("typed delayed fan_out_request with multiple results") {
  scoped_actor self{sys};
  std::vector<typed_worker_two_values_actor> workers{
    make_typed_server_two_values(
      sys, [](int x, int y) { return result<int, int>(y, x); }),
    make_typed_server_two_values(
      sys, [](int x, int y) { return result<int, int>(y, x); }),
    make_typed_server_two_values(
      sys, [](int x, int y) { return result<int, int>(y, x); }),
  };
  auto swapped_values = std::make_shared<std::vector<std::pair<int, int>>>();
  auto single_result = std::make_shared<std::pair<int, int>>();
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive(
        [swapped_values](std::vector<std::tuple<int, int>> results) {
          for (auto result : results) {
            swapped_values->emplace_back(std::get<0>(result),
                                         std::get<1>(result));
          }
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(swapped_values->size(), 3u);
    for (const auto& pair : *swapped_values) {
      check_eq(pair.first, 2);
      check_eq(pair.second, 1);
    }
    check_eq(*err, error{});
  }
  SECTION("receive with policy select_any") {
    self->mail(3, 5)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive(
        [single_result](int first, int second) {
          *single_result = std::make_pair(first, second);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(single_result->first, 5);
    check_eq(single_result->second, 3);
    check_eq(*err, error{});
  }
  SECTION("receive with policy select_all returning an expected") {
    auto swapped_values = self->mail(1, 2)
                            .delay(100ms)
                            .fan_out_request(workers, 1s,
                                             policy::select_all_tag)
                            .receive();
    check_has_value(swapped_values);
    check_eq(swapped_values->size(), 3u);
    for (const auto& pair : *swapped_values) {
      check_eq(std::get<0>(pair), 2);
      check_eq(std::get<1>(pair), 1);
    }
  }
  SECTION("receive with policy select_any returning an expected") {
    auto swapped_values = self->mail(3, 5)
                            .delay(100ms)
                            .fan_out_request(workers, 1s,
                                             policy::select_any_tag)
                            .receive();
    check_has_value(swapped_values);
    check_eq(std::get<0>(*swapped_values), 5);
    check_eq(std::get<1>(*swapped_values), 3);
  }
}

TEST("typed delayed fan_out_request with void result") {
  scoped_actor self{sys};
  std::vector<typed_worker_void_actor> workers{
    make_typed_server_void(sys, [](int, int) {}),
    make_typed_server_void(sys, [](int, int) {}),
    make_typed_server_void(sys, [](int, int) {}),
  };
  auto ran = std::make_shared<bool>(false);
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive([ran]() { *ran = true; },
               [err](error& e) { *err = std::move(e); });
    check(*ran);
    check_eq(*err, error{});
  }
  SECTION("receive with policy select_any") {
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive([ran]() { *ran = true; },
               [err](error& e) { *err = std::move(e); });
    check(*ran);
    check_eq(*err, error{});
  }
}

TEST("fan_out_request with one worker returning an error") {
  scoped_actor self{sys};
  std::vector<actor> workers{
    make_server(sys, [](int x, int y) { return x + y; }),
    make_server(sys,
                [](int, int) -> result<int> {
                  return caf::error(sec::logic_error);
                }),
    make_server(sys, [](int x, int y) { return x + y; }),
  };
  auto sum = std::make_shared<int>(0);
  auto err = std::make_shared<error>();
  SECTION("receive with policy select_all") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_all_tag)
      .receive(
        [this](std::vector<int> results) {
          fail("expected an error, got: {}", results);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(*err, make_error(sec::logic_error));
  }
  SECTION("receive with policy select_any") {
    self->mail(1, 2)
      .fan_out_request(workers, 1s, policy::select_any_tag)
      .receive([sum](int result) { *sum = result; },
               [err](error& e) { *err = std::move(e); });
    check_eq(*sum, 3);
    check_eq(*err, error{});
  }
}

TEST("delayed fan_out_request with timeout") {
  scoped_actor self{sys};
  auto dummy = [](event_based_actor* self) -> behavior {
    auto res = std::make_shared<response_promise>();
    return {
      [self, res](int, int) {
        *res = self->make_response_promise();
        return *res;
      },
    };
  };
  std::vector<actor> workers{sys.spawn(dummy), sys.spawn(dummy),
                             sys.spawn(dummy)};
  auto sum = std::make_shared<int>(0);
  auto err = std::make_shared<error>();
  SECTION("timeout with policy select_all") {
    auto start = std::chrono::steady_clock::now();
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 100ms, policy::select_all_tag)
      .receive(
        [sum, this](std::vector<int> results) {
          for (auto result : results)
            check_eq(result, 3);
          *sum = std::accumulate(results.begin(), results.end(), 0);
        },
        [err](error& e) { *err = std::move(e); });
    check_eq(*err, make_error(sec::request_timeout));
    auto elapsed = std::chrono::steady_clock::now() - start;
    check_ge(elapsed, 200ms);
    check_le(elapsed, 220ms);
  }
  SECTION("timeout with policy select_any") {
    auto start = std::chrono::steady_clock::now();
    self->mail(1, 2)
      .delay(100ms)
      .fan_out_request(workers, 100ms, policy::select_any_tag)
      .receive([sum](int result) { *sum = result; },
               [err](error& e) { *err = std::move(e); });
    check_eq(*err, make_error(sec::request_timeout));
    auto elapsed = std::chrono::steady_clock::now() - start;
    check_ge(elapsed, 200ms);
    check_le(elapsed, 220ms);
  }
  for (const auto& worker : workers) {
    self->send_exit(worker, exit_reason::user_shutdown);
  }
}

} // WITH_FIXTURE(fixture)
