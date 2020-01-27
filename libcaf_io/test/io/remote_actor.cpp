/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE io.remote_actor

#include "caf/test/io_dsl.hpp"

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;

namespace {

struct suite_state {
  int pings = 0;
  int pongs = 0;
  error linking_result;
  suite_state() = default;
};

using suite_state_ptr = std::shared_ptr<suite_state>;

behavior ping(event_based_actor* self, suite_state_ptr ssp) {
  return {
    [=](ok_atom, const actor& pong) {
      CAF_MESSAGE("received `ok_atom`");
      ++ssp->pings;
      self->send(pong, ping_atom_v);
      self->become([=](pong_atom) {
        CAF_MESSAGE("ping: received pong");
        self->send(pong, ping_atom_v);
        if (++ssp->pings == 10) {
          self->quit();
          CAF_MESSAGE("ping is done");
        }
      });
    },
  };
}

behavior pong(event_based_actor* self, suite_state_ptr ssp) {
  return {
    [=](ping_atom) -> pong_atom {
      CAF_MESSAGE("pong: received ping");
      if (++ssp->pongs == 10) {
        self->quit();
        CAF_MESSAGE("pong is done");
      }
      return pong_atom_v;
    },
  };
}

using fragile_mirror_actor = typed_actor<replies_to<int>::with<int>>;

fragile_mirror_actor::behavior_type
fragile_mirror(fragile_mirror_actor::pointer self) {
  return {
    [=](int i) {
      self->quit(exit_reason::user_shutdown);
      return i;
    },
  };
}

behavior linking_actor(event_based_actor* self,
                       const fragile_mirror_actor& buddy, suite_state_ptr ssp) {
  CAF_MESSAGE("link to mirror and send dummy message");
  self->send(buddy, 42);
  self->link_to(buddy);
  self->set_exit_handler([=](exit_msg& msg) {
    // Record exit reason for checking it later.
    ssp->linking_result = msg.reason;
    self->quit(std::move(msg.reason));
  });
  return {
    [](int i) { CAF_CHECK_EQUAL(i, 42); },
  };
}

struct fixture : point_to_point_fixture<> {
  fixture() {
    prepare_connection(mars, earth, "mars", 8080);
    ssp = std::make_shared<suite_state>();
  }

  suite_state_ptr ssp;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(dynamic_remote_actor_tests, fixture)

CAF_TEST(identity_semantics) {
  auto server = mars.sys.spawn(pong, ssp);
  auto port = mars.publish(server, 8080);
  CAF_CHECK_EQUAL(port, 8080u);
  auto same_server = earth.remote_actor("mars", 8080);
  CAF_REQUIRE_EQUAL(same_server, server);
  anon_send_exit(server, exit_reason::user_shutdown);
}

CAF_TEST(ping_pong) {
  auto port = mars.publish(mars.sys.spawn(pong, ssp), 8080);
  CAF_CHECK_EQUAL(port, 8080u);
  auto remote_pong = earth.remote_actor("mars", 8080);
  anon_send(earth.sys.spawn(ping, ssp), ok_atom_v, remote_pong);
  run();
  CAF_CHECK_EQUAL(ssp->pings, 10);
  CAF_CHECK_EQUAL(ssp->pongs, 10);
}

CAF_TEST(remote_link) {
  auto port = mars.publish(mars.sys.spawn(fragile_mirror), 8080);
  CAF_CHECK_EQUAL(port, 8080u);
  auto mirror = earth.remote_actor<fragile_mirror_actor>("mars", 8080);
  earth.sys.spawn(linking_actor, mirror, ssp);
  run();
  CAF_CHECK_EQUAL(ssp->linking_result, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
