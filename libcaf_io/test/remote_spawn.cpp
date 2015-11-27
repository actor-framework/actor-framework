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

#include "caf/detail/run_sub_unit_test.hpp"

#ifdef CAF_USE_ASIO
#include "caf/io/network/asio_multiplexer.hpp"
#endif // CAF_USE_ASIO

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

} // namespace <anonymous>

CAF_TEST(remote_spawn) {
  auto argv = test::engine::argv();
  auto argc = test::engine::argc();
  uint16_t port = 0;
  auto r = message_builder(argv, argv + argc).extract_opts({
    {"server,s", "run as server (don't run client"},
    {"client,c", "add client port (two needed)", port},
    {"port,p", "force a port in server mode", port},
    {"use-asio", "use ASIO network backend (if available)"}
  });
  if (! r.error.empty() || r.opts.count("help") > 0 || ! r.remainder.empty()) {
    std::cout << r.error << std::endl << std::endl << r.helptext << std::endl;
    return;
  }
  actor_system_config cfg;
  cfg.add_actor_type("mirror", mirror);
  auto use_asio = r.opts.count("use-asio") > 0;
# ifdef CAF_USE_ASIO
  if (use_asio)
    cfg.load<io::middleman, io::network::asio_multiplexer>();
  else
# endif // CAF_USE_ASIO
    cfg.load<io::middleman>();
  actor_system system{cfg};
  if (r.opts.count("client") > 0) {
    auto serv = system.middleman().remote_actor("localhost", port);
    CAF_REQUIRE(serv);
    system.spawn(client, serv);
    system.await_all_actors_done();
    return;
  }
  auto serv = system.spawn(server);
  auto mport = system.middleman().publish(serv, port);
  CAF_REQUIRE(mport);
  port = *mport;
  CAF_MESSAGE("published server at port " << port);
  if (r.opts.count("server") == 0) {
    CAF_MESSAGE("run client program");
    auto child = detail::run_sub_unit_test(invalid_actor,
                                           test::engine::path(),
                                           test::engine::max_runtime(),
                                           CAF_XSTR(CAF_SUITE),
                                           use_asio,
                                           {"--client="
                                            + std::to_string(port)});
    child.join();
  }
  system.await_all_actors_done();
}
