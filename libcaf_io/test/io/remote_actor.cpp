// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE io.remote_actor

#include "io-test.hpp"

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
      MESSAGE("received `ok_atom`");
      ++ssp->pings;
      self->send(pong, ping_atom_v);
      self->become([=](pong_atom) {
        MESSAGE("ping: received pong");
        self->send(pong, ping_atom_v);
        if (++ssp->pings == 10) {
          self->quit();
          MESSAGE("ping is done");
        }
      });
    },
  };
}

behavior pong(event_based_actor* self, suite_state_ptr ssp) {
  return {
    [=](ping_atom) -> pong_atom {
      MESSAGE("pong: received ping");
      if (++ssp->pongs == 10) {
        self->quit();
        MESSAGE("pong is done");
      }
      return pong_atom_v;
    },
  };
}

using fragile_mirror_actor = typed_actor<result<int>(int)>;

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
  MESSAGE("link to mirror and send dummy message");
  self->send(buddy, 42);
  self->link_to(buddy);
  self->set_exit_handler([=](exit_msg& msg) {
    // Record exit reason for checking it later.
    ssp->linking_result = msg.reason;
    self->quit(std::move(msg.reason));
  });
  return {
    [](int i) { CHECK_EQ(i, 42); },
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

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(identity_semantics) {
  auto server = mars.sys.spawn(pong, ssp);
  auto port = mars.publish(server, 8080);
  CHECK_EQ(port, 8080u);
  auto same_server = earth.remote_actor("mars", 8080);
  CAF_REQUIRE_EQUAL(same_server, server);
  anon_send_exit(server, exit_reason::user_shutdown);
}

CAF_TEST(ping_pong) {
  auto port = mars.publish(mars.sys.spawn(pong, ssp), 8080);
  CHECK_EQ(port, 8080u);
  auto remote_pong = earth.remote_actor("mars", 8080);
  anon_send(earth.sys.spawn(ping, ssp), ok_atom_v, remote_pong);
  run();
  CHECK_EQ(ssp->pings, 10);
  CHECK_EQ(ssp->pongs, 10);
}

CAF_TEST(remote_link) {
  auto port = mars.publish(mars.sys.spawn(fragile_mirror), 8080);
  CHECK_EQ(port, 8080u);
  auto mirror = earth.remote_actor<fragile_mirror_actor>("mars", 8080);
  earth.sys.spawn(linking_actor, mirror, ssp);
  run();
  CHECK_EQ(ssp->linking_result, exit_reason::user_shutdown);
}

END_FIXTURE_SCOPE()
