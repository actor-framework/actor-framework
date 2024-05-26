// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/response_promise.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

behavior adder() {
  return {
    [](int x, int y) { return x + y; },
    [](ok_atom) {},
  };
}

behavior delegator(event_based_actor* self, actor worker) {
  return {
    [=](int x, int y) {
      auto promise = self->make_response_promise();
      return promise.delegate(worker, x, y);
    },
    [=](ok_atom) {
      auto promise = self->make_response_promise();
      return promise.delegate(worker, ok_atom_v);
    },
  };
}

behavior requester_v1(event_based_actor* self, actor worker) {
  return {
    [=](int x, int y) {
      auto rp = self->make_response_promise();
      self->request(worker, infinite, x, y)
        .then(
          [rp](int result) mutable {
            test::runnable::current().check(rp.pending());
            rp.deliver(result);
          },
          [rp](error err) mutable {
            test::runnable::current().check(rp.pending());
            rp.deliver(std::move(err));
          });
      return rp;
    },
    [=](ok_atom) {
      auto rp = self->make_response_promise();
      self->request(worker, infinite, ok_atom_v)
        .then(
          [rp]() mutable {
            test::runnable::current().check(rp.pending());
            rp.deliver();
          },
          [rp](error err) mutable {
            test::runnable::current().check(rp.pending());
            rp.deliver(std::move(err));
          });
      return rp;
    },
  };
}

behavior requester_v2(event_based_actor* self, actor worker) {
  return {
    [=](int x, int y) {
      auto rp = self->make_response_promise();
      auto deliver = [rp](expected<int> x) mutable {
        test::runnable::current().check(rp.pending());
        rp.deliver(std::move(x));
      };
      self->request(worker, infinite, x, y)
        .then([deliver](int result) mutable { deliver(result); },
              [deliver](error err) mutable { deliver(std::move(err)); });
      return rp;
    },
    [=](ok_atom) {
      auto rp = self->make_response_promise();
      auto deliver = [rp](expected<void> x) mutable {
        test::runnable::current().check(rp.pending());
        rp.deliver(std::move(x));
      };
      self->request(worker, infinite, ok_atom_v)
        .then([deliver]() mutable { deliver({}); },
              [deliver](error err) mutable { deliver(std::move(err)); });
      return rp;
    },
  };
}

WITH_FIXTURE(test::fixture::deterministic) {

SCENARIO("response promises allow delaying of response messages") {
  auto adder_hdl = sys.spawn(adder);
  std::map<std::string, actor> impls;
  impls["with a value or an error"] = sys.spawn(requester_v1, adder_hdl);
  impls["with an expected<T>"] = sys.spawn(requester_v2, adder_hdl);
  scoped_actor self{sys};
  for (auto& [desc, hdl] : impls) {
    GIVEN("a dispatcher that calls deliver " + desc + "on its promise") {
      WHEN("sending a request with two integers to the dispatcher") {
        inject().with(3, 4).from(self).to(hdl);
        THEN("clients receive the response from the dispatcher") {
          expect<int, int>().with(3, 4).from(hdl).to(adder_hdl);
          expect<int>().with(7).from(adder_hdl).to(hdl);
          auto received = std::make_shared<bool>(false);
          self->receive([this, received](int received_int) {
            *received = true;
            check_eq(received_int, 7);
          });
          check(*received);
          dispatch_messages();
        }
      }
      WHEN("sending ok_atom to the dispatcher synchronously") {
        auto res = self->request(hdl, infinite, ok_atom_v);
        auto fetch_result = [&] {
          message result;
          res.receive([] {}, // void result
                      [&](const error& reason) {
                        result = make_message(reason);
                      });
          return result;
        };
        THEN("clients receive an empty response from the dispatcher") {
          expect<ok_atom>().from(self).to(hdl);
          expect<ok_atom>().from(hdl).to(adder_hdl);
          dispatch_message();
          check(fetch_result().empty());
        }
      }
      WHEN("sending ok_atom to the dispatcher asynchronously") {
        THEN("clients receive no response from the dispatcher") {
          inject().with(ok_atom_v).from(self).to(hdl);
          expect<ok_atom>().from(hdl).to(adder_hdl);
          dispatch_message();
          check(self->mailbox().empty());
        }
      }
    }
  }
}

SCENARIO("response promises send errors when broken") {
  auto adder_hdl = sys.spawn(adder);
  auto hdl = sys.spawn(requester_v1, adder_hdl);
  scoped_actor self{sys};
  GIVEN("a dispatcher, and adder and a client") {
    WHEN("the dispatcher terminates before calling deliver on its promise") {
      inject().with(3, 4).from(self).to(hdl);
      inject().with(exit_msg{hdl.address(), exit_reason::kill}).to(hdl);
      THEN("clients receive a broken_promise error") {
        auto received = std::make_shared<bool>(false);
        self->receive([this, received](error e) {
          *received = true;
          check_eq(e, sec::broken_promise);
        });
        check(*received);
      }
    }
  }
}

SCENARIO("response promises allow delegation") {
  GIVEN("a dispatcher that calls delegate on its promise") {
    scoped_actor self{sys};
    auto adder_hdl = sys.spawn(adder);
    auto hdl = sys.spawn(delegator, adder_hdl);
    WHEN("sending a request to the dispatcher") {
      inject().with(3, 4).from(self).to(hdl);
      THEN("clients receive the response from the adder") {
        expect<int, int>().with(3, 4).from(self).to(adder_hdl);
        auto received = std::make_shared<bool>(false);
        self->receive([this, received](int received_int) {
          *received = true;
          check_eq(received_int, 7);
        });
        check(*received);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

TEST("GH - 1306 regression") {
  actor_system_config cfg;
  cfg.set("caf.scheduler.max-threads", 1u);
  actor_system sys{cfg};
  auto aut = sys.spawn([](caf::event_based_actor* self) -> behavior {
    return {
      [self](int32_t x) {
        auto rp = self->make_response_promise();
        self->run_delayed(1h, [rp, x]() mutable { rp.deliver(x + x); });
        return rp;
      },
    };
  });
  scoped_actor self{sys};
  self->mail(21).send(aut);
  self->send_exit(aut, exit_reason::kill);
  aut = nullptr;
  // Destroying the now obsolete action now destroys the promise. If the promise
  // access the self pointer this triggers a heap-use-after-free since the AUT
  // has been destroyed at this point.
}

} // namespace
