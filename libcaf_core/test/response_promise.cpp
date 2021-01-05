/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2021 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE response_promise

#include "caf/response_promise.hpp"

#include "core-test.hpp"

using namespace caf;

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
            CHECK(rp.pending());
            rp.deliver(result);
          },
          [rp](error err) mutable {
            CHECK(rp.pending());
            rp.deliver(std::move(err));
          });
      return rp;
    },
    [=](ok_atom) {
      auto rp = self->make_response_promise();
      self->request(worker, infinite, ok_atom_v)
        .then(
          [rp]() mutable {
            CHECK(rp.pending());
            rp.deliver();
          },
          [rp](error err) mutable {
            CHECK(rp.pending());
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
        CHECK(rp.pending());
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
        CHECK(rp.pending());
        rp.deliver(std::move(x));
      };
      self->request(worker, infinite, ok_atom_v)
        .then([deliver]() mutable { deliver({}); },
              [deliver](error err) mutable { deliver(std::move(err)); });
      return rp;
    },
  };
}

} // namespace

CAF_TEST_FIXTURE_SCOPE(response_promise_tests, test_coordinator_fixture<>)

SCENARIO("response promises allow delaying of response messages") {
  auto adder_hdl = sys.spawn(adder);
  std::map<std::string, actor> impls;
  impls["with a value or an error"] = sys.spawn(requester_v1, adder_hdl);
  impls["with an expected<T>"] = sys.spawn(requester_v2, adder_hdl);
  for (auto& [desc, hdl] : impls) {
    GIVEN("a dispatcher that calls deliver " << desc << " on its promise") {
      WHEN("sending a request with two integers to the dispatcher") {
        inject((int, int), from(self).to(hdl).with(3, 4));
        THEN("clients receive the response from the dispatcher") {
          expect((int, int), from(hdl).to(adder_hdl).with(3, 4));
          expect((int), from(adder_hdl).to(hdl).with(7));
          expect((int), from(hdl).to(self).with(7));
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
          expect((ok_atom), from(self).to(hdl));
          expect((ok_atom), from(hdl).to(adder_hdl));
          expect((void), from(adder_hdl).to(hdl));
          CHECK(fetch_result().empty());
        }
      }
      WHEN("sending ok_atom to the dispatcher asynchronously") {
        THEN("clients receive no response from the dispatcher") {
          inject((ok_atom), from(self).to(hdl).with(ok_atom_v));
          expect((ok_atom), from(hdl).to(adder_hdl));
          expect((void), from(adder_hdl).to(hdl));
          CHECK(self->mailbox().empty());
        }
      }
    }
  }
}

SCENARIO("response promises allow delegation") {
  GIVEN("a dispatcher that calls delegate on its promise") {
    auto adder_hdl = sys.spawn(adder);
    auto hdl = sys.spawn(delegator, adder_hdl);
    WHEN("sending a request to the dispatcher") {
      inject((int, int), from(self).to(hdl).with(3, 4));
      THEN("clients receive the response from the adder") {
        expect((int, int), from(self).to(adder_hdl).with(3, 4));
        expect((int), from(adder_hdl).to(self).with(7));
      }
    }
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
