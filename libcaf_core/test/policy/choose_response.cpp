/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE policy.choose_response

#include "caf/policy/choose_response.hpp"

#include "caf/test/dsl.hpp"

#include "caf/actor_system.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/sec.hpp"

using caf::policy::choose_response;

using namespace caf;

namespace {

struct fixture : test_coordinator_fixture<> {
  template <class F>
  actor make_server(F f) {
    auto init = [f]() -> behavior {
      return {
        [f](int x, int y) { return f(x, y); },
      };
    };
    return sys.spawn(init);
  }

  auto make_error_handler() {
    return [this](const error& err) {
      CAF_FAIL("unexpected error: " << sys.render(err));
    };
  }

  auto make_counting_error_handler(size_t* count) {
    return [count](const error&) { *count += 1; };
  }
};

} // namespace

#define SUBTEST(message)                                                       \
  run();                                                                       \
  CAF_MESSAGE("subtest: " message);                                            \
  for (int subtest_dummy = 0; subtest_dummy < 1; ++subtest_dummy)

CAF_TEST_FIXTURE_SCOPE(choose_response_tests, fixture)

CAF_TEST(choose_response picks the first arriving integer) {
  auto f = [](int x, int y) { return x + y; };
  auto server1 = make_server(f);
  auto server2 = make_server(f);
  SUBTEST("request.receive") {
    SUBTEST("single integer") {
      auto r1 = self->request(server1, infinite, 1, 2);
      auto r2 = self->request(server2, infinite, 2, 3);
      choose_response<detail::type_list<int>> choose{{r1.id(), r2.id()}};
      run();
      choose.receive(
        self.ptr(), [](int result) { CAF_CHECK_EQUAL(result, 3); },
        make_error_handler());
    }
  }
  SUBTEST("request.then") {
    int result = 0;
    auto client = sys.spawn([=, &result](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      choose_response<detail::type_list<int>> choose{{r1.id(), r2.id()}};
      choose.then(
        client_ptr, [&result](int x) { result = x; }, make_error_handler());
    });
    run_once();
    expect((int, int), from(client).to(server1).with(1, 2));
    expect((int, int), from(client).to(server2).with(2, 3));
    expect((int), from(server1).to(client).with(3));
    expect((int), from(server2).to(client).with(5));
    CAF_MESSAGE("request.then picks the first arriving result");
    CAF_CHECK_EQUAL(result, 3);
  }
  SUBTEST("request.await") {
    int result = 0;
    auto client = sys.spawn([=, &result](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      choose_response<detail::type_list<int>> choose{{r1.id(), r2.id()}};
      choose.await(
        client_ptr, [&result](int x) { result = x; }, make_error_handler());
    });
    run_once();
    expect((int, int), from(client).to(server1).with(1, 2));
    expect((int, int), from(client).to(server2).with(2, 3));
    // TODO: DSL (mailbox.peek) cannot deal with receivers that skip messages.
    // expect((int), from(server1).to(client).with(3));
    // expect((int), from(server2).to(client).with(5));
    run();
    CAF_MESSAGE("request.await froces responses into reverse request order");
    CAF_CHECK_EQUAL(result, 5);
  }
}

CAF_TEST(choose_response calls the error handler at most once) {
  auto f = [](int, int) -> result<int> { return sec::invalid_argument; };
  auto server1 = make_server(f);
  auto server2 = make_server(f);
  SUBTEST("request.receive") {
    auto r1 = self->request(server1, infinite, 1, 2);
    auto r2 = self->request(server2, infinite, 2, 3);
    choose_response<detail::type_list<int>> choose{{r1.id(), r2.id()}};
    run();
    size_t errors = 0;
    choose.receive(
      self.ptr(),
      [](int) { CAF_FAIL("fan-in policy called the result handler"); },
      make_counting_error_handler(&errors));
    CAF_CHECK_EQUAL(errors, 1u);
  }
  SUBTEST("request.then") {
    size_t errors = 0;
    auto client = sys.spawn([=, &errors](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      choose_response<detail::type_list<int>> choose{{r1.id(), r2.id()}};
      choose.then(
        client_ptr,
        [](int) { CAF_FAIL("fan-in policy called the result handler"); },
        make_counting_error_handler(&errors));
    });
    run_once();
    expect((int, int), from(client).to(server1).with(1, 2));
    expect((int, int), from(client).to(server2).with(2, 3));
    expect((error), from(server1).to(client).with(sec::invalid_argument));
    expect((error), from(server2).to(client).with(sec::invalid_argument));
    CAF_CHECK_EQUAL(errors, 1u);
  }
  SUBTEST("request.await") {
    size_t errors = 0;
    auto client = sys.spawn([=, &errors](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      choose_response<detail::type_list<int>> choose{{r1.id(), r2.id()}};
      choose.await(
        client_ptr,
        [](int) { CAF_FAIL("fan-in policy called the result handler"); },
        make_counting_error_handler(&errors));
    });
    run_once();
    expect((int, int), from(client).to(server1).with(1, 2));
    expect((int, int), from(client).to(server2).with(2, 3));
    // TODO: DSL (mailbox.peek) cannot deal with receivers that skip messages.
    // expect((int), from(server1).to(client).with(3));
    // expect((int), from(server2).to(client).with(5));
    run();
    CAF_CHECK_EQUAL(errors, 1u);
  }
}

CAF_TEST_FIXTURE_SCOPE_END()
