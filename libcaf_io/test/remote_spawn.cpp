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

#include "caf/experimental/announce_actor_type.hpp"

#ifdef CAF_USE_ASIO
#include "caf/io/network/asio_multiplexer.hpp"
#endif // CAF_USE_ASIO

using namespace caf;
using namespace caf::experimental;

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
      CAF_TEST_ERROR("unexpected message: "
                     << to_string(self->current_message()));
    }
  };
}

struct server_state {
  actor client; // the spawn we connect to the server in our main
  actor aut; // our mirror
};

behavior server(stateful_actor<server_state>* self) {
  self->on_sync_failure([=] {
    CAF_TEST_ERROR("unexpected sync response: "
                   << to_string(self->current_message()));
  });
  return {
    [=](ok_atom) {
      auto s = self->current_sender();
      CAF_REQUIRE(s != invalid_actor_addr);
      CAF_REQUIRE(s.is_remote());
      self->state.client = actor_cast<actor>(s);
      auto mm = io::get_middleman_actor();
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
  announce_actor_type("mirror", mirror);
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
  auto use_asio = r.opts.count("use-asio") > 0;
# ifdef CAF_USE_ASIO
  if (use_asio) {
    CAF_MESSAGE("enable ASIO backend");
    io::set_middleman<io::network::asio_multiplexer>();
  }
# endif // CAF_USE_ASIO
  if (r.opts.count("client") > 0) {
    auto serv = io::remote_actor("localhost", port);
    spawn(client, serv);
    await_all_actors_done();
    return;
  }
  auto serv = spawn(server);
  port = io::publish(serv, port);
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
  await_all_actors_done();
  shutdown();
}
