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

#include "caf/config.hpp"

#define CAF_SUITE io_dynamic_remote_group
#include "caf/test/unit_test.hpp"

#include <vector>
#include <algorithm>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;

namespace {

constexpr char local_host[] = "127.0.0.1";

class config : public caf::actor_system_config {
public:
  config() {
    load<caf::io::middleman>();
    add_message_type<std::vector<actor>>("std::vector<actor>");
  }

  config& parse() {
    actor_system_config::parse(caf::test::engine::argc(),
                               caf::test::engine::argv());
    return *this;
  }
};

struct fixture {
  config server_side_cfg;
  caf::actor_system server_side{server_side_cfg.parse()};
  config client_side_cfg;
  caf::actor_system client_side{client_side_cfg.parse()};
  io::middleman& server_side_mm = server_side.middleman();
  io::middleman& client_side_mm = client_side.middleman();
};

behavior make_reflector_behavior(event_based_actor* self) {
  self->set_default_handler(reflect_and_quit);
  return {
    [] {
      // nop
    }
  };
}

using spawn_atom = atom_constant<atom("Spawn")>;
using get_group_atom = atom_constant<atom("GetGroup")>;

struct await_reflector_reply_behavior {
  event_based_actor* self;
  int cnt;
  int downs;
  std::vector<actor> vec;

  void operator()(const std::string& str, double val) {
    CAF_CHECK_EQUAL(str, "Hello reflector!");
    CAF_CHECK_EQUAL(val, 5.0);
    if (++cnt == 7) {
      for (const auto& actor : vec)
        self->monitor(actor);
      self->set_down_handler([=](down_msg&) {
        if (++downs == 5)
          self->quit();
      });
    }
  }
};

// `grp` may be either local or remote
void make_client_behavior(event_based_actor* self,
                          const actor& server, group grp) {
  self->set_default_handler(skip);
  self->spawn_in_group(grp, make_reflector_behavior);
  self->spawn_in_group(grp, make_reflector_behavior);
  self->request(server, infinite, spawn_atom::value, grp).then(
    [=](const std::vector<actor>& vec) {
      auto is_remote = [=](actor actor) {
        return actor->node() != self->node();
      };
      CAF_CHECK(std::all_of(vec.begin(), vec.end(), is_remote));
      self->send(grp, "Hello reflector!", 5.0);
      self->become(await_reflector_reply_behavior{self, 0, 0, vec});
    }
  );
}

behavior make_server_behavior(event_based_actor* self) {
  return {
    [=](get_group_atom) {
      return self->system().groups().get_local("foobar");
    },
    [=](spawn_atom, group group) -> std::vector<actor> {
      std::vector<actor> vec;
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

CAF_TEST_DISABLED(remote_group_conn) {
  // server side
  CAF_EXP_THROW(port, server_side_mm.publish_local_groups(0));
  CAF_REQUIRE(port != 0);
  // client side
  CAF_CHECK(client_side_mm.remote_group("whatever", local_host, port));
}

CAF_TEST_DISABLED(server_side_group_comm) {
  // server side
  CAF_EXP_THROW(port,
                server_side_mm.publish(server_side.spawn(make_server_behavior),
                                       0, local_host));
  CAF_REQUIRE(port != 0);
  // client side
  CAF_EXP_THROW(server, client_side_mm.remote_actor(local_host, port));
  scoped_actor group_resolver(client_side, true);
  group grp;
  group_resolver->request(server, infinite, get_group_atom::value).receive(
    [&](const group& x) {
      grp = x;
    },
    [&](error& err) {
      CAF_FAIL("error: " << client_side.render(err));
    }
  );
  client_side.spawn(make_client_behavior, server, grp);
}

CAF_TEST_DISABLED(client_side_group_comm) {
  // server side
  CAF_EXP_THROW(port,
                server_side_mm.publish(server_side.spawn(make_server_behavior),
                                       0, local_host));
  CAF_REQUIRE(port != 0);
  // client side
  CAF_EXP_THROW(server, client_side_mm.remote_actor(local_host, port));
  client_side.spawn(make_client_behavior, server,
                    client_side.groups().get_local("foobar"));
}

CAF_TEST_FIXTURE_SCOPE_END()
