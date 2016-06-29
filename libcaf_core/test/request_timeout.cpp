/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE request_timeout
#include "caf/test/unit_test.hpp"

#include <thread>
#include <chrono>

#include "caf/all.hpp"

using namespace caf;

using std::chrono::seconds;
using std::chrono::milliseconds;

namespace {

using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;
using send_ping_atom = atom_constant<atom("send_ping")>;

behavior pong() {
  return {
    [=] (ping_atom) {
      std::this_thread::sleep_for(seconds(1));
      return pong_atom::value;
    }
  };
}

behavior ping1(event_based_actor* self, const actor& pong_actor) {
  self->link_to(pong_actor);
  self->send(self, send_ping_atom::value);
  return {
    [=](send_ping_atom) {
      self->request(pong_actor, milliseconds(100), ping_atom::value).then(
        [=](pong_atom) {
          CAF_ERROR("received pong atom");
          self->quit(exit_reason::user_shutdown);
        },
        [=](const error& err) {
          CAF_REQUIRE(err == sec::request_timeout);
          self->quit(exit_reason::user_shutdown);
        }
      );
    }
  };
}

behavior ping2(event_based_actor* self, const actor& pong_actor) {
  self->link_to(pong_actor);
  self->send(self, send_ping_atom::value);
  auto received_inner = std::make_shared<bool>(false);
  return {
    [=](send_ping_atom) {
      self->request(pong_actor, milliseconds(100), ping_atom::value).then(
        [=](pong_atom) {
          CAF_ERROR("received pong atom");
          self->quit(exit_reason::user_shutdown);
        },
        [=](const error& err) {
          CAF_REQUIRE(err == sec::request_timeout);
          CAF_MESSAGE("inner timeout: check");
          *received_inner = true;
        }
      );
    },
    after(milliseconds(100)) >> [=] {
      CAF_CHECK_EQUAL(*received_inner, true);
      self->quit(exit_reason::user_shutdown);
    }
  };
}

behavior ping3(event_based_actor* self, const actor& pong_actor) {
  self->link_to(pong_actor);
  self->send(self, send_ping_atom::value);
  return {
    [=](send_ping_atom) {
      self->request(pong_actor, milliseconds(100),
                    ping_atom::value).then(
        [=](pong_atom) {
          CAF_ERROR("received pong atom");
          self->quit(exit_reason::user_shutdown);
        },
        [=](const error& err) {
          CAF_REQUIRE(err == sec::request_timeout);
          CAF_MESSAGE("async timeout: check");
          self->quit(exit_reason::user_shutdown);
        }
      );
    }
  };
}

behavior ping4(event_based_actor* self, const actor& pong_actor) {
  self->link_to(pong_actor);
  self->send(self, send_ping_atom::value);
  auto received_outer = std::make_shared<bool>(false);
  return {
    [=](send_ping_atom) {
      self->request(pong_actor, milliseconds(100),
                    ping_atom::value).then(
        [=](pong_atom) {
          CAF_ERROR("received pong atom");
          self->quit(exit_reason::user_shutdown);
        },
        [=](const error& err) {
          CAF_REQUIRE(err == sec::request_timeout);
          CAF_CHECK_EQUAL(*received_outer, true);
          self->quit(exit_reason::user_shutdown);
        }
      );
    },
    after(milliseconds(50)) >> [=] {
      CAF_MESSAGE("outer timeout: check");
      *received_outer = true;
    }
  };
}

void ping5(event_based_actor* self, const actor& pong_actor) {
  self->link_to(pong_actor);
  auto timeouts = std::make_shared<int>(0);
  self->request(pong_actor, milliseconds(100),
                ping_atom::value).then(
    [=](pong_atom) {
      CAF_ERROR("received pong atom");
    },
    [=](const error& err) {
      CAF_REQUIRE(err == sec::request_timeout);
      if (++*timeouts == 2)
        self->quit();
    }
  );
  self->request(pong_actor, milliseconds(100),
                ping_atom::value).await(
    [=](pong_atom) {
      CAF_ERROR("received pong atom");
    },
    [=](const error& err) {
      CAF_REQUIRE(err == sec::request_timeout);
      if (++*timeouts == 2)
        self->quit();
    }
  );
}

struct fixture {
  fixture() : system(cfg) {
    // nop
  }

  actor_system_config cfg;
  actor_system system;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(request_timeout_tests, fixture)

CAF_TEST(single_timeout) {
  system.spawn(ping1, system.spawn(pong));
  system.spawn(ping3, system.spawn(pong));
}

CAF_TEST(scoped_timeout) {
  system.spawn(ping2, system.spawn(pong));
  system.spawn(ping4, system.spawn(pong));
}

CAF_TEST(awaited_multiplexed_timeout) {
  system.spawn(ping5, system.spawn(pong));
}

CAF_TEST_FIXTURE_SCOPE_END()
