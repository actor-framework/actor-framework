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

#define CAF_SUITE io_dynamic_remote_group
#include "caf/test/unit_test.hpp"

#include <vector>
#include <algorithm>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

namespace {

constexpr char local_host[] = "127.0.0.1";

caf::actor_system_config make_actor_system_config() {
  caf::actor_system_config cfg(caf::test::engine::argc(),
                               caf::test::engine::argv());
  cfg.load<caf::io::middleman>();
  cfg.add_message_type<std::vector<caf::actor>>("std::vector<caf::actor>");
  return cfg;
}

struct fixture {
  caf::actor_system server_side{make_actor_system_config()};
  caf::actor_system client_side{make_actor_system_config()};
  caf::io::middleman& server_side_mm = server_side.middleman();
  caf::io::middleman& client_side_mm = client_side.middleman();
};

caf::behavior make_reflector_behavior(caf::event_based_actor* self) {
  return {
    caf::others >> [=] {
      self->quit();
      return self->current_message();
    }
  };
}

using spawn_atom = caf::atom_constant<caf::atom("Spawn")>;
using get_group_atom = caf::atom_constant<caf::atom("GetGroup")>;

struct await_reflector_down_behavior {
  caf::event_based_actor* self;
  int cnt;

  void operator()(const caf::down_msg&) {
    if (++cnt == 5)
      self->quit();
  }
};

struct await_reflector_reply_behavior {
  caf::event_based_actor* self;
  int cnt;

  void operator()(const std::string& str, double val) {
    CAF_CHECK_EQUAL(str, "Hello reflector!");
    CAF_CHECK_EQUAL(val, 5.0);
    if (++cnt == 7)
      self->become(await_reflector_down_behavior{self, 0});
  }
};

// `grp` may be either local or remote
void make_client_behavior(caf::event_based_actor* self,
                          caf::actor server, caf::group grp) {
  self->spawn_in_group(grp, make_reflector_behavior);
  self->spawn_in_group(grp, make_reflector_behavior);
  self->request(server, spawn_atom::value, grp).then(
    [=](const std::vector<caf::actor>& vec) {
      auto is_remote = [=](caf::actor actor) {
        return actor->node() != self->node();
      };
      CAF_CHECK(std::all_of(vec.begin(), vec.end(), is_remote));
      self->send(grp, "Hello reflector!", 5.0);
      for (auto actor : vec) {
        self->monitor(actor);
      }
      self->become(await_reflector_reply_behavior{self, 0});
    }
  );
}

caf::behavior make_server_behavior(caf::event_based_actor* self) {
  return {
    [=](get_group_atom) {
      return self->system().groups().get("local", "foobar");
    },
    [=](spawn_atom, caf::group group) -> std::vector<caf::actor> {
      std::vector<caf::actor> vec;
      for (auto i = 0; i < 5; ++i) {
        vec.push_back(self->spawn_in_group(group, make_reflector_behavior));
      }
      self->quit();
      return vec;
    }
  };
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(dynamic_remote_group_tests, fixture)

CAF_TEST(remote_group_conn) {
  // server side
  auto port = server_side_mm.publish_local_groups(0);
  CAF_REQUIRE(port && *port);
  // client side
  CAF_CHECK(client_side_mm.remote_group("whatever", local_host, *port));
}

CAF_TEST(server_side_group_comm) {
  // server side
  auto port = server_side_mm.publish(server_side.spawn(make_server_behavior),
                                     0, local_host);
  CAF_REQUIRE(port && *port);
  // client side
  auto server = client_side_mm.remote_actor(local_host, *port);
  CAF_REQUIRE(server);
  caf::scoped_actor group_resolver(client_side, true);
  caf::group group;
  group_resolver->request(server, get_group_atom::value).receive(
    [&](caf::group grp) {
      group = grp;
    }
  );
  client_side.spawn(make_client_behavior, server, group);
}

CAF_TEST(client_side_group_comm) {
  // server side
  auto port = server_side_mm.publish(server_side.spawn(make_server_behavior),
                                     0, local_host);
  CAF_REQUIRE(port && *port);
  // client side
  auto server = client_side_mm.remote_actor(local_host, *port);
  CAF_REQUIRE(server);
  client_side.spawn(make_client_behavior, server,
                    client_side.groups().get("local", "foobar"));
}

CAF_TEST_FIXTURE_SCOPE_END()
