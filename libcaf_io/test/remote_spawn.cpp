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
  self->set_default_handler(reflect);
  return {
    [] {
      // nop
    }
  };
}

behavior client(event_based_actor* self, actor serv) {
  self->send(serv, ok_atom::value);
  return {
    [] {
      // nop
    }
  };
}

struct server_state {
  actor client; // the spawn we connect to the server in our main
  actor aut; // our mirror
  server_state()
      : client(unsafe_actor_handle_init),
        aut(unsafe_actor_handle_init) {
    // nop
  }
};

behavior server(stateful_actor<server_state>* self) {
  CAF_LOG_TRACE("");
  return {
    [=](ok_atom) {
      CAF_LOG_TRACE("");
      auto s = self->current_sender();
      CAF_REQUIRE(s != nullptr);
      CAF_REQUIRE(self->node() != s->node());
      auto opt = actor_cast<actor>(s);
      //CAF_REQUIRE(opt);
      self->state.client = opt;
      auto mm = self->system().middleman().actor_handle();
      self->request(mm, infinite, spawn_atom::value,
                    s->node(), "mirror", make_message()).then(
        [=](ok_atom, const strong_actor_ptr& ptr,
            const std::set<std::string>& ifs) {
          CAF_LOG_TRACE(CAF_ARG(ptr) << CAF_ARG(ifs));
          CAF_REQUIRE(ptr);
          CAF_CHECK(ifs.empty());
          self->state.aut = actor_cast<actor>(ptr);
          self->send(self->state.aut, "hello mirror");
          self->become(
            [=](const std::string& str) {
              CAF_CHECK_EQUAL(self->current_sender(),
                              self->state.aut.address());
              CAF_CHECK_EQUAL(str, "hello mirror");
              self->send_exit(self->state.aut, exit_reason::kill);
              self->send_exit(self->state.client, exit_reason::kill);
              self->quit();
            }
          );
        }
      );
    },
    [=](const error& err) {
      CAF_LOG_TRACE("");
      CAF_ERROR("Error: " << self->system().render(err));
    }
  };
}

void run_client(int argc, char** argv, uint16_t port) {
  actor_system_config cfg;
  cfg.parse(argc, argv)
     .load<io::middleman>()
     .add_actor_type("mirror", mirror);
  actor_system system{cfg};
  CAF_EXP_THROW(serv, system.middleman().remote_actor("localhost", port));
  system.spawn(client, serv);
}

void run_server(int argc, char** argv) {
  actor_system_config cfg;
  actor_system system{cfg.load<io::middleman>().parse(argc, argv)};
  auto serv = system.spawn(server);
  CAF_EXP_THROW(port, system.middleman().publish(serv, 0));
  CAF_REQUIRE(port != 0);
  CAF_MESSAGE("published server at port " << port);
  std::thread child([=] { run_client(argc, argv, port); });
  child.join();
}

} // namespace <anonymous>

CAF_TEST(remote_spawn) {
  auto argc = test::engine::argc();
  auto argv = test::engine::argv();
  run_server(argc, argv);
}
