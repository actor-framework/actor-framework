// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE policy.select_any

#include "caf/policy/select_any.hpp"

#include "caf/actor_system.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/sec.hpp"

#include "core-test.hpp"

using caf::policy::select_any;

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
    return [](const error& err) { CAF_FAIL("unexpected error: " << err); };
  }

  auto make_counting_error_handler(size_t* count) {
    return [count](const error&) { *count += 1; };
  }

  template <class... ResponseHandles>
  auto fuse(ResponseHandles&... handles) {
    return select_any<detail::type_list<int>>{
      {handles.id()...},
      disposable::make_composite({handles.policy().pending_timeouts()...})};
  }
};

} // namespace

#define SUBTEST(message)                                                       \
  run();                                                                       \
  MESSAGE("subtest: " message);                                                \
  for (int subtest_dummy = 0; subtest_dummy < 1; ++subtest_dummy)

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(select_any picks the first arriving integer) {
  auto f = [](int x, int y) { return x + y; };
  auto server1 = make_server(f);
  auto server2 = make_server(f);
  SUBTEST("request.receive") {
    SUBTEST("single integer") {
      auto r1 = self->request(server1, infinite, 1, 2);
      auto r2 = self->request(server2, infinite, 2, 3);
      auto choose = fuse(r1, r2);
      run();
      choose.receive(
        self.ptr(), [](int result) { CHECK_EQ(result, 3); },
        make_error_handler());
    }
  }
  SUBTEST("request.then") {
    int result = 0;
    auto client = sys.spawn([=, &result](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      auto choose = fuse(r1, r2);
      choose.then(
        client_ptr, [&result](int x) { result = x; }, make_error_handler());
    });
    run_once();
    expect((int, int), from(client).to(server1).with(1, 2));
    expect((int, int), from(client).to(server2).with(2, 3));
    expect((int), from(server1).to(client).with(3));
    expect((int), from(server2).to(client).with(5));
    MESSAGE("request.then picks the first arriving result");
    CHECK_EQ(result, 3);
  }
  SUBTEST("request.await") {
    int result = 0;
    auto client = sys.spawn([=, &result](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      auto choose = fuse(r1, r2);
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
    MESSAGE("request.await froces responses into reverse request order");
    CHECK_EQ(result, 5);
  }
}

CAF_TEST(select_any calls the error handler at most once) {
  auto f = [](int, int) -> result<int> { return sec::invalid_argument; };
  auto server1 = make_server(f);
  auto server2 = make_server(f);
  SUBTEST("request.receive") {
    auto r1 = self->request(server1, infinite, 1, 2);
    auto r2 = self->request(server2, infinite, 2, 3);
    auto choose = fuse(r1, r2);
    run();
    size_t errors = 0;
    choose.receive(
      self.ptr(),
      [](int) { CAF_FAIL("fan-in policy called the result handler"); },
      make_counting_error_handler(&errors));
    CHECK_EQ(errors, 1u);
  }
  SUBTEST("request.then") {
    size_t errors = 0;
    auto client = sys.spawn([=, &errors](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      auto choose = fuse(r1, r2);
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
    CHECK_EQ(errors, 1u);
  }
  SUBTEST("request.await") {
    size_t errors = 0;
    auto client = sys.spawn([=, &errors](event_based_actor* client_ptr) {
      auto r1 = client_ptr->request(server1, infinite, 1, 2);
      auto r2 = client_ptr->request(server2, infinite, 2, 3);
      auto choose = fuse(r1, r2);
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
    CHECK_EQ(errors, 1u);
  }
}

END_FIXTURE_SCOPE()
