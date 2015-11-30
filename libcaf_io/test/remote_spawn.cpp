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

#define CAF_SUITE io_remote_spawn
#include "caf/test/unit_test.hpp"

#include <thread>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <functional>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;

namespace {

behavior mirror(event_based_actor* self) {
  return {
    others >> [=] {
      return self->current_message();
    }
  };
}

behavior client(event_based_actor* self, actor serv) {
  self->send(serv, ok_atom::value);
  return {
    others >> [=] {
      CAF_TEST_ERROR("Unexpected message");
    }
  };
}

struct server_state {
  actor client; // the spawn we connect to the server in our main
  actor aut; // our mirror
};

behavior server(stateful_actor<server_state>* self) {
  self->on_sync_failure([=] {
    CAF_TEST_ERROR("Unexpected sync response");
  });
  return {
    [=](ok_atom) {
      auto s = self->current_sender();
      CAF_REQUIRE(s != invalid_actor_addr);
      CAF_REQUIRE(self->node() != s.node());
      self->state.client = actor_cast<actor>(s);
      auto mm = self->system().middleman().actor_handle();
      self->sync_send(mm, spawn_atom::value,
                      s.node(), "mirror", make_message()).then(
        [=](ok_atom, const actor_addr& addr, const std::set<std::string>& ifs) {
          CAF_REQUIRE(addr != invalid_actor_addr);
          CAF_CHECK(ifs.empty());
          self->state.aut = actor_cast<actor>(addr);
          self->send(self->state.aut, "hello mirror");
          self->become(
            [=](const std::string& str) {
              CAF_CHECK(self->current_sender() == self->state.aut);
              CAF_CHECK(str == "hello mirror");
              self->send_exit(self->state.aut, exit_reason::kill);
              self->send_exit(self->state.client, exit_reason::kill);
              self->quit();
            }
          );
        },
        [=](error_atom, const std::string& errmsg) {
          CAF_TEST_ERROR("could not spawn mirror: " << errmsg);
        }
      );
    }
  };
}

void run_client(int argc, char** argv, uint16_t port) {
  actor_system_config cfg{argc, argv};
  cfg.load<io::middleman>()
     .add_actor_type("mirror", mirror);
  actor_system system{cfg};
  auto serv = system.middleman().remote_actor("localhost", port);
  CAF_REQUIRE(serv);
  system.spawn(client, serv);
  system.await_all_actors_done();
}

void run_server(int argc, char** argv) {
  actor_system system{actor_system_config{argc, argv}.load<io::middleman>()};
  auto serv = system.spawn(server);
  auto mport = system.middleman().publish(serv, 0);
  CAF_REQUIRE(mport);
  auto port = *mport;
  CAF_MESSAGE("published server at port " << port);
  std::thread child([=] { run_client(argc, argv, port); });
  system.await_all_actors_done();
  child.join();
}

} // namespace <anonymous>

CAF_TEST(remote_spawn) {
  auto argc = test::engine::argc();
  auto argv = test::engine::argv();
  run_server(argc, argv);
}
