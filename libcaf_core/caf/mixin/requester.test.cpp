// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/mixin/requester.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/log/test.hpp"
#include "caf/policy/select_all.hpp"
#include "caf/result.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

#include <numeric>

using namespace caf;
using namespace std::literals;

namespace {

using discarding_server_type = typed_actor<result<void>(int, int)>;

using adding_server_type = typed_actor<result<int>(int, int)>;
using no_op_server_type = typed_actor<result<void>(int, int)>;

using result_type = std::variant<none_t, unit_t, int>;

struct fixture : test::fixture::deterministic {
  scoped_actor self{sys};
  template <class F>
  auto make_server(F f) {
    using res_t = caf::result<decltype(f(1, 2))>;
    using impl = typed_actor<res_t(int, int)>;
    auto init = [f]() -> typename impl::behavior_type {
      return {
        [f](int x, int y) { return f(x, y); },
      };
    };
    return sys.spawn(init);
  }

  fixture() {
    result = std::make_shared<result_type>(none);
    discarding_server = make_server([](int, int) {});
    adding_server = make_server([](int x, int y) { return x + y; });
    dispatch_messages();
  }

  template <class T>
  T make_delegator(T dest) {
    auto f = [=](typename T::pointer self) -> typename T::behavior_type {
      return {
        [=](int x, int y) { return self->delegate(dest, x, y); },
      };
    };
    return sys.spawn<lazy_init>(f);
  }

  std::shared_ptr<result_type> result = std::make_shared<result_type>(none);
  discarding_server_type discarding_server;
  adding_server_type adding_server;
};

#define ERROR_HANDLER                                                          \
  [&](error& err) { test::runnable::current().fail("{}", err); }

#define SUBTEST(message)                                                       \
  *result = none;                                                              \
  dispatch_messages();                                                         \
  log::test::debug("subtest: {}", message);                                    \
  for (int subtest_dummy = 0; subtest_dummy < 1; ++subtest_dummy)

WITH_FIXTURE(fixture) {

TEST("requests without result") {
  auto server = discarding_server;
  SUBTEST("request.then") {
    auto client = sys.spawn([this, server](event_based_actor* self) {
      self->request(server, infinite, 1, 2).then([this] { *result = unit; });
    });
    expect<int, int>().with(1, 2).from(client).to(server);
    dispatch_messages();
    check_eq(*result, result_type{unit});
  }
  SUBTEST("request.await") {
    auto client = sys.spawn([this, server](event_based_actor* self) {
      self->request(server, infinite, 1, 2).await([this] { *result = unit; });
    });
    expect<int, int>().with(1, 2).from(client).to(server);
    dispatch_messages();
    check_eq(*result, result_type{unit});
  }
  SUBTEST("request.receive") {
    auto res_hdl = self->request(server, infinite, 1, 2);
    dispatch_messages();
    res_hdl.receive([&] { *result = unit; }, ERROR_HANDLER);
    check_eq(*result, result_type{unit});
  }
}

TEST("requests with integer result") {
  auto server = adding_server;
  SUBTEST("request.then") {
    auto client = sys.spawn([this, server](event_based_actor* self) {
      self->request(server, infinite, 1, 2).then([this](int x) {
        *result = x;
      });
    });
    expect<int, int>().with(1, 2).from(client).to(server);
    expect<int>().with(3).from(server).to(client);
    check_eq(*result, result_type{3});
  }
  SUBTEST("request.await") {
    auto client = sys.spawn([this, server](event_based_actor* self) {
      self->request(server, infinite, 1, 2).await([this](int x) {
        *result = x;
      });
    });
    expect<int, int>().with(1, 2).from(client).to(server);
    expect<int>().with(3).from(server).to(client);
    check_eq(*result, result_type{3});
  }
  SUBTEST("request.receive") {
    auto res_hdl = self->request(server, infinite, 1, 2);
    dispatch_messages();
    res_hdl.receive([&](int x) { *result = x; }, ERROR_HANDLER);
    check_eq(*result, result_type{3});
  }
}

TEST("delegated request with integer result") {
  auto worker = adding_server;
  auto server = make_delegator(worker);
  auto client = sys.spawn([this, server](event_based_actor* self) {
    self->request(server, infinite, 1, 2).then([this](int x) { *result = x; });
  });
  expect<int, int>().with(1, 2).from(client).to(server);
  expect<int, int>().with(1, 2).from(client).to(worker);
  expect<int>().with(3).from(worker).to(client);
  check_eq(*result, result_type{3});
}

TEST("requesters support fan_out_request") {
  using policy::select_all;
  std::vector<adding_server_type> workers{
    make_server([](int x, int y) { return x + y; }),
    make_server([](int x, int y) { return x + y; }),
    make_server([](int x, int y) { return x + y; }),
  };
  dispatch_messages();
  auto sum = std::make_shared<int>(0);
  auto client = sys.spawn([workers, sum](event_based_actor* self) {
    self->fan_out_request<select_all>(workers, infinite, 1, 2)
      .then([=](std::vector<int> results) {
        for (auto result : results)
          test::runnable::current().check_eq(result, 3);
        *sum = std::accumulate(results.begin(), results.end(), 0);
      });
  });
  expect<int, int>().with(1, 2).from(client).to(workers[0]);
  expect<int>().with(3).from(workers[0]).to(client);
  expect<int, int>().with(1, 2).from(client).to(workers[1]);
  expect<int>().with(3).from(workers[1]).to(client);
  expect<int, int>().with(1, 2).from(client).to(workers[2]);
  expect<int>().with(3).from(workers[2]).to(client);
  check_eq(*sum, 9);
}

TEST("requesters support fan_out_request with void result") {
  using policy::select_all;
  std::vector<no_op_server_type> workers{
    make_server([](int, int) {}),
    make_server([](int, int) {}),
    make_server([](int, int) {}),
  };
  auto ran = std::make_shared<bool>(false);
  auto client = sys.spawn([=](event_based_actor* self) {
    self->fan_out_request<select_all>(workers, infinite, 1, 2).then([=]() {
      *ran = true;
    });
  });
  expect<int, int>().with(1, 2).from(client).to(workers[0]);
  expect<int, int>().with(1, 2).from(client).to(workers[1]);
  expect<int, int>().with(1, 2).from(client).to(workers[2]);
  dispatch_messages();
  check_eq(*ran, true);
}

#ifdef CAF_ENABLE_EXCEPTIONS

TEST("exceptions while processing requests trigger error messages") {
  auto worker = sys.spawn([] {
    return behavior{
      [](int) { throw std::runtime_error(""); },
    };
  });
  dispatch_messages();
  auto client = sys.spawn([worker](event_based_actor* self) {
    self->request(worker, infinite, 42).then([](int) {
      test::runnable::current().fail("unexpected handler called");
    });
  });
  expect<int>().with(42).from(client).to(worker);
  expect<error>().with(sec::runtime_error).from(worker).to(client);
}

#endif // CAF_ENABLE_EXCEPTIONS

SCENARIO("request.await enforces a processing order") {
  GIVEN("an actor that is waiting for a request.await handler") {
    auto server = sys.spawn([]() -> behavior {
      return {
        [](int32_t x) { return x * x; },
      };
    });
    dispatch_messages();
    auto received = std::make_shared<bool>(false);
    auto client
      = sys.spawn([server, received](event_based_actor* self) -> behavior {
          self->request(server, infinite, int32_t{3})
            .await([received](int32_t res) {
              test::runnable::current().check_eq(res, 9);
              *received = true;
            });
          return {
            [](const std::string& str) {
              // Received from self.
              test::runnable::current().check_eq(str, "hello");
            },
          };
        });
    WHEN("sending it a message before the response arrives") {
      THEN("the actor handles the asynchronous message later") {
        self->mail("hello").send(client);
        disallow<std::string>().from(self).to(client);     // not processed yet
        expect<int32_t>().with(3).from(client).to(server); // client -> server
        disallow<std::string>().from(self).to(client);     // not processed yet
        dispatch_message();                                // server -> client
        check(*received);
        expect<std::string>().with("hello").from(self).to(client); // at last
      }
    }
  }
}

// The GH-1299 worker processes int32 and string messages but alternates between
// processing either type.

using log_ptr = std::shared_ptr<std::string>;

behavior gh1299_worker_bhvr1(caf::event_based_actor* self, log_ptr log);

behavior gh1299_worker_bhvr2(caf::event_based_actor* self, log_ptr log);

behavior gh1299_worker(caf::event_based_actor* self, log_ptr log) {
  self->set_default_handler(skip);
  return gh1299_worker_bhvr1(self, log);
}

behavior gh1299_worker_bhvr1(caf::event_based_actor* self, log_ptr log) {
  return {
    [self, log](int32_t x) {
      *log += "int: ";
      *log += std::to_string(x);
      *log += '\n';
      self->become(gh1299_worker_bhvr2(self, log));
    },
  };
}

behavior gh1299_worker_bhvr2(caf::event_based_actor* self, log_ptr log) {
  return {
    [self, log](const std::string& x) {
      *log += "string: ";
      *log += x;
      *log += '\n';
      self->become(gh1299_worker_bhvr1(self, log));
    },
  };
}

TEST("GH-1299 regression non-blocking") {
  SUBTEST("HIGH (skip) -> NORMAL") {
    auto log = std::make_shared<std::string>();
    auto worker = sys.spawn(gh1299_worker, log);
    scoped_actor self{sys};
    self->mail("hi there").urgent().send(worker);
    dispatch_messages();
    self->mail(int32_t{123}).send(worker);
    dispatch_messages();
    check_eq(*log, "int: 123\nstring: hi there\n");
  }
  SUBTEST("NORMAL (skip) -> HIGH") {
    auto log = std::make_shared<std::string>();
    auto worker = sys.spawn(gh1299_worker, log);
    scoped_actor self{sys};
    self->mail("hi there").send(worker);
    dispatch_messages();
    self->mail(int32_t{123}).urgent().send(worker);
    dispatch_messages();
    check_eq(*log, "int: 123\nstring: hi there\n");
  }
}

void gh1299_recv(scoped_actor& self, log_ptr log, int& tag) {
  bool fin = false;
  for (;;) {
    if (tag == 0) {
      self->receive(
        [log, &tag](int32_t x) {
          *log += "int: ";
          *log += std::to_string(x);
          *log += '\n';
          tag = 1;
        },
        after(timespan{0}) >> [&fin] { fin = true; });
      if (fin)
        return;
    } else {
      self->receive(
        [log, &tag](const std::string& x) {
          *log += "string: ";
          *log += x;
          *log += '\n';
          tag = 0;
        },
        after(timespan{0}) >> [&fin] { fin = true; });
      if (fin)
        return;
    }
  }
}

TEST("GH-1299 regression blocking") {
  SUBTEST("HIGH (skip) -> NORMAL") {
    auto log = std::make_shared<std::string>();
    auto tag = 0;
    scoped_actor sender{sys};
    scoped_actor self{sys};
    sender->mail("hi there").urgent().send(self);
    gh1299_recv(self, log, tag);
    sender->mail(int32_t{123}).send(self);
    gh1299_recv(self, log, tag);
    check_eq(*log, "int: 123\nstring: hi there\n");
  }
  SUBTEST("NORMAL (skip) -> HIGH") {
    auto log = std::make_shared<std::string>();
    auto tag = 0;
    scoped_actor sender{sys};
    scoped_actor self{sys};
    sender->mail("hi there").send(self);
    gh1299_recv(self, log, tag);
    sender->mail(int32_t{123}).urgent().send(self);
    gh1299_recv(self, log, tag);
    check_eq(*log, "int: 123\nstring: hi there\n");
  }
}

TEST("GH-698 regression") {
  auto server = actor_cast<actor>(adding_server);
  auto client = actor_cast<strong_actor_ptr>(
    sys.spawn([](event_based_actor* self) -> behavior {
      return [self](actor server) {
        self->request(server, 10s, 1, 2).then([](int) {});
      };
    }));
  dispatch_messages();
  check_eq(client->strong_refs, 1u);
  inject().with(server).from(server).to(client);
  check(has_pending_timeout());
  expect<int, int>().from(client).to(server);
  expect<int>().from(server).to(client);
  check(!has_pending_timeout());
  // The scheduler may no longer hold a reference to the client.
  check_eq(client->strong_refs, 1u);
}

} // WITH_FIXTURE(fixture)

} // namespace
