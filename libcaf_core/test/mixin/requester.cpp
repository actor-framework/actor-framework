// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE mixin.requester

#include "caf/mixin/requester.hpp"

#include "core-test.hpp"

#include <numeric>

#include "caf/event_based_actor.hpp"
#include "caf/policy/select_all.hpp"
#include "caf/result.hpp"

using namespace caf;

namespace {

using discarding_server_type = typed_actor<result<void>(int, int)>;

using adding_server_type = typed_actor<result<int>(int, int)>;

using result_type = std::variant<none_t, unit_t, int>;

struct fixture : test_coordinator_fixture<> {
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
    run();
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

} // namespace

#define ERROR_HANDLER [&](error& err) { CAF_FAIL(err); }

#define SUBTEST(message)                                                       \
  *result = none;                                                              \
  run();                                                                       \
  MESSAGE("subtest: " message);                                                \
  for (int subtest_dummy = 0; subtest_dummy < 1; ++subtest_dummy)

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(requests without result) {
  auto server = discarding_server;
  SUBTEST("request.then") {
    auto client = sys.spawn([=](event_based_actor* self) {
      self->request(server, infinite, 1, 2).then([=] { *result = unit; });
    });
    run_once();
    expect((int, int), from(client).to(server).with(1, 2));
    expect((void), from(server).to(client));
    CHECK_EQ(*result, result_type{unit});
  }
  SUBTEST("request.await") {
    auto client = sys.spawn([=](event_based_actor* self) {
      self->request(server, infinite, 1, 2).await([=] { *result = unit; });
    });
    run_once();
    expect((int, int), from(client).to(server).with(1, 2));
    expect((void), from(server).to(client));
    CHECK_EQ(*result, result_type{unit});
  }
  SUBTEST("request.receive") {
    auto res_hdl = self->request(server, infinite, 1, 2);
    run();
    res_hdl.receive([&] { *result = unit; }, ERROR_HANDLER);
    CHECK_EQ(*result, result_type{unit});
  }
}

CAF_TEST(requests with integer result) {
  auto server = adding_server;
  SUBTEST("request.then") {
    auto client = sys.spawn([=](event_based_actor* self) {
      self->request(server, infinite, 1, 2).then([=](int x) { *result = x; });
    });
    run_once();
    expect((int, int), from(client).to(server).with(1, 2));
    expect((int), from(server).to(client).with(3));
    CHECK_EQ(*result, result_type{3});
  }
  SUBTEST("request.await") {
    auto client = sys.spawn([=](event_based_actor* self) {
      self->request(server, infinite, 1, 2).await([=](int x) { *result = x; });
    });
    run_once();
    expect((int, int), from(client).to(server).with(1, 2));
    expect((int), from(server).to(client).with(3));
    CHECK_EQ(*result, result_type{3});
  }
  SUBTEST("request.receive") {
    auto res_hdl = self->request(server, infinite, 1, 2);
    run();
    res_hdl.receive([&](int x) { *result = x; }, ERROR_HANDLER);
    CHECK_EQ(*result, result_type{3});
  }
}

CAF_TEST(delegated request with integer result) {
  auto worker = adding_server;
  auto server = make_delegator(worker);
  auto client = sys.spawn([=](event_based_actor* self) {
    self->request(server, infinite, 1, 2).then([=](int x) { *result = x; });
  });
  run_once();
  expect((int, int), from(client).to(server).with(1, 2));
  expect((int, int), from(client).to(worker).with(1, 2));
  expect((int), from(worker).to(client).with(3));
  CHECK_EQ(*result, result_type{3});
}

CAF_TEST(requesters support fan_out_request) {
  using policy::select_all;
  std::vector<adding_server_type> workers{
    make_server([](int x, int y) { return x + y; }),
    make_server([](int x, int y) { return x + y; }),
    make_server([](int x, int y) { return x + y; }),
  };
  run();
  auto sum = std::make_shared<int>(0);
  auto client = sys.spawn([=](event_based_actor* self) {
    self->fan_out_request<select_all>(workers, infinite, 1, 2)
      .then([=](std::vector<int> results) {
        for (auto result : results)
          CHECK_EQ(result, 3);
        *sum = std::accumulate(results.begin(), results.end(), 0);
      });
  });
  run_once();
  expect((int, int), from(client).to(workers[0]).with(1, 2));
  expect((int), from(workers[0]).to(client).with(3));
  expect((int, int), from(client).to(workers[1]).with(1, 2));
  expect((int), from(workers[1]).to(client).with(3));
  expect((int, int), from(client).to(workers[2]).with(1, 2));
  expect((int), from(workers[2]).to(client).with(3));
  CHECK_EQ(*sum, 9);
}

#ifdef CAF_ENABLE_EXCEPTIONS

CAF_TEST(exceptions while processing requests trigger error messages) {
  auto worker = sys.spawn([] {
    return behavior{
      [](int) { throw std::runtime_error(""); },
    };
  });
  run();
  auto client = sys.spawn([worker](event_based_actor* self) {
    self->request(worker, infinite, 42).then([](int) {
      CAF_FAIL("unexpected handler called");
    });
  });
  run_once();
  expect((int), from(client).to(worker).with(42));
  expect((error), from(worker).to(client).with(sec::runtime_error));
}

#endif // CAF_ENABLE_EXCEPTIONS

SCENARIO("request.await enforces a processing order") {
  GIVEN("an actor that is waiting for a request.await handler") {
    auto server = sys.spawn([]() -> behavior {
      return {
        [](int32_t x) { return x * x; },
      };
    });
    run();
    auto client = sys.spawn([server](event_based_actor* self) -> behavior {
      self->request(server, infinite, int32_t{3}).await([](int32_t res) {
        CHECK_EQ(res, 9);
      });
      return {
        [](const std::string& str) {
          // Received from self.
          CHECK_EQ(str, "hello");
        },
      };
    });
    sched.run_once();
    WHEN("sending it a message before the response arrives") {
      THEN("the actor handles the asynchronous message later") {
        self->send(client, "hello");
        disallow((std::string), from(self).to(client));     // not processed yet
        expect((int32_t), from(client).to(server).with(3)); // client -> server
        disallow((std::string), from(self).to(client));     // not processed yet
        expect((int32_t), from(server).to(client).with(9)); // server -> client
        expect((std::string), from(self).to(client).with("hello")); // at last
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

TEST_CASE("GH-1299 regression non-blocking") {
  SUBTEST("HIGH (skip) -> NORMAL") {
    auto log = std::make_shared<std::string>();
    auto worker = sys.spawn(gh1299_worker, log);
    scoped_actor self{sys};
    self->send<message_priority::high>(worker, "hi there");
    run();
    self->send(worker, int32_t{123});
    run();
    CHECK_EQ(*log, "int: 123\nstring: hi there\n");
  }
  SUBTEST("NORMAL (skip) -> HIGH") {
    auto log = std::make_shared<std::string>();
    auto worker = sys.spawn(gh1299_worker, log);
    scoped_actor self{sys};
    self->send(worker, "hi there");
    run();
    self->send<message_priority::high>(worker, int32_t{123});
    run();
    CHECK_EQ(*log, "int: 123\nstring: hi there\n");
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

TEST_CASE("GH-1299 regression blocking") {
  SUBTEST("HIGH (skip) -> NORMAL") {
    auto log = std::make_shared<std::string>();
    auto tag = 0;
    scoped_actor sender{sys};
    scoped_actor self{sys};
    sender->send<message_priority::high>(self, "hi there");
    gh1299_recv(self, log, tag);
    sender->send(self, int32_t{123});
    gh1299_recv(self, log, tag);
    CHECK_EQ(*log, "int: 123\nstring: hi there\n");
  }
  SUBTEST("NORMAL (skip) -> HIGH") {
    auto log = std::make_shared<std::string>();
    auto tag = 0;
    scoped_actor sender{sys};
    scoped_actor self{sys};
    sender->send(self, "hi there");
    gh1299_recv(self, log, tag);
    sender->send<message_priority::high>(self, int32_t{123});
    gh1299_recv(self, log, tag);
    CHECK_EQ(*log, "int: 123\nstring: hi there\n");
  }
}

END_FIXTURE_SCOPE()
