// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/log/test.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

namespace {

behavior mirror_impl(event_based_actor* self) {
  self->set_default_handler(reflect);
  return [] {
    // nop
  };
}

struct fixture : test::fixture::deterministic {
  actor mirror;
  actor testee;
  scoped_actor self{sys};

  fixture() {
    mirror = sys.spawn(mirror_impl);
    // run initialization code or mirror
    dispatch_message();
  }

  template <class... Ts>
  void spawn(Ts&&... xs) {
    testee = self->spawn(std::forward<Ts>(xs)...);
  }

  ~fixture() {
    self->wait_for(testee);
  }
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("single multiplexed request") {
  auto f = [this](event_based_actor* self, actor server) {
    self->mail(42).request(server, infinite).then([this](int x) {
      auto lg = log::core::trace("x = {}", x);
      require_eq(x, 42);
    });
  };
  spawn(f, mirror);
  expect<int>().with(42).from(testee).to(mirror);
  expect<int>().with(42).from(mirror).to(testee);
}

TEST("multiple multiplexed requests") {
  auto f = [this](event_based_actor* self, actor server) {
    for (int i = 0; i < 3; ++i)
      self->mail(42).request(server, infinite).then([this](int x) {
        auto lg = log::core::trace("x = {}", x);
        require_eq(x, 42);
      });
  };
  spawn(f, mirror);
  expect<int>().with(42).from(testee).to(mirror);
  expect<int>().with(42).from(testee).to(mirror);
  expect<int>().with(42).from(testee).to(mirror);
  expect<int>().with(42).from(mirror).to(testee);
  expect<int>().with(42).from(mirror).to(testee);
  expect<int>().with(42).from(mirror).to(testee);
}

TEST("single awaited request") {
  auto f = [this](event_based_actor* self, actor server) {
    self->mail(42).request(server, infinite).await([this](int x) {
      require_eq(x, 42);
    });
  };
  spawn(f, mirror);
  expect<int>().with(42).from(testee).to(mirror);
  expect<int>().with(42).from(mirror).to(testee);
}

TEST("multiple awaited requests") {
  auto f = [this](event_based_actor* self, actor server) {
    for (int i = 0; i < 3; ++i)
      self->mail(i).request(server, infinite).await([this, i](int x) {
        log::test::debug("received response #{}", (i + 1));
        require_eq(x, i);
      });
  };
  spawn(f, mirror);
  self->monitor(testee);
  expect<int>().with(0).from(testee).to(mirror);
  expect<int>().with(1).from(testee).to(mirror);
  expect<int>().with(2).from(testee).to(mirror);
  // request().await() processes messages out-of-order,
  // which means we cannot check using expect()
  dispatch_messages();
  auto received_down_msg = std::make_shared<bool>(false);
  self->receive([received_down_msg](down_msg&) { *received_down_msg = true; });
  check(*received_down_msg);
}

SCENARIO("actors clean up incoming edges when they terminate") {
  GIVEN("a client that monitors a server using the legacy API") {
    auto server = sys.spawn([] {
      return behavior{
        [](int value) { return value + 1; },
      };
    });
    auto client = sys.spawn([server](event_based_actor* self) {
      self->monitor(server);
      return behavior{
        [self, server](int value) {
          return self->mail(value).delegate(server);
        },
      };
    });
    auto wptr = actor_cast<weak_actor_ptr>(client);
    // only the `client` variable holds a strong reference to the client actor
    check_eq(client->ctrl()->strong_reference_count(), 1u);
    // implicit ref (from the strong refs) + the ref from the monitor + `wptr`
    check_eq(client->ctrl()->weak_reference_count(), 3u);
    WHEN("the client terminates") {
      client = nullptr; // will be cleaned up as unreachable
      check_eq(mail_count(), 0u);
      THEN("the server no longer holds a weak reference to the client") {
        check_eq(wptr.ctrl()->strong_reference_count(), 0u);
        check_eq(wptr.ctrl()->weak_reference_count(), 1u);
      }
    }
  }
  GIVEN("a client that monitors a server using the callback API") {
    auto server = sys.spawn([] {
      return behavior{
        [](int value) { return value + 1; },
      };
    });
    auto client = sys.spawn([server](event_based_actor* self) {
      self->monitor(server, [](const error&) {});
      return behavior{
        [self, server](int value) {
          return self->mail(value).delegate(server);
        },
      };
    });
    auto wptr = actor_cast<weak_actor_ptr>(client);
    // only the `client` variable holds a strong reference to the client actor
    check_eq(client->ctrl()->strong_reference_count(), 1u);
    // implicit ref (from the strong refs) + the ref from the monitor + `wptr`
    check_eq(client->ctrl()->weak_reference_count(), 3u);
    WHEN("the client terminates") {
      client = nullptr; // will be cleaned up as unreachable
      check_eq(mail_count(), 0u);
      THEN("the server no longer holds a weak reference to the client") {
        check_eq(wptr.ctrl()->strong_reference_count(), 0u);
        check_eq(wptr.ctrl()->weak_reference_count(), 1u);
      }
    }
  }
  GIVEN("a client that links to a server") {
    auto server = sys.spawn([] {
      return behavior{
        [](int value) { return value + 1; },
      };
    });
    auto client = sys.spawn([server](event_based_actor* self) {
      self->link_to(server);
      return behavior{
        [self, server](int value) {
          return self->mail(value).delegate(server);
        },
      };
    });
    auto wptr = actor_cast<weak_actor_ptr>(client);
    // only the `client` variable holds a strong reference to the client actor
    check_eq(client->ctrl()->strong_reference_count(), 1u);
    // implicit ref + `wptr` + one backlink attachable on the server
    check_eq(client->ctrl()->weak_reference_count(), 3u);
    WHEN("the client terminates") {
      client = nullptr; // will be cleaned up as unreachable
      check_eq(mail_count(), 1u);
      expect<exit_msg>().to(server);
      check_eq(mail_count(), 0u);
      THEN("the server no longer holds a weak reference to the client") {
        check_eq(wptr.ctrl()->strong_reference_count(), 0u);
        check_eq(wptr.ctrl()->weak_reference_count(), 1u);
      }
    }
  }
}

SCENARIO("disposing monitor actions removes the attachable") {
  GIVEN("a client that monitors a server") {
    auto server = sys.spawn([] {
      return behavior{
        [](int value) { return value + 1; },
      };
    });
    auto client = sys.spawn([server](event_based_actor* self) {
      auto hdl = self->monitor(server, [](const error&) {});
      return behavior{
        [hdl](ok_atom) mutable { hdl.dispose(); },
        [self, server](int value) {
          return self->mail(value).delegate(server);
        },
      };
    });
    auto wptr = actor_cast<weak_actor_ptr>(client);
    // only the `client` variable holds a strong reference to the client actor
    check_eq(client->ctrl()->strong_reference_count(), 1u);
    // implicit ref (from the strong refs) + the ref from the monitor + `wptr`
    check_eq(client->ctrl()->weak_reference_count(), 3u);
    WHEN("the client disposes the monitor action") {
      inject().with(ok_atom_v).to(client);
      THEN("the server no longer holds a weak reference to the client") {
        check_eq(client->ctrl()->strong_reference_count(), 1u);
        check_eq(client->ctrl()->weak_reference_count(), 2u);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
