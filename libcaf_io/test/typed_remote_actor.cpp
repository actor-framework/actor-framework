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

#define CAF_SUITE io_typed_remote_actor
#include "caf/test/dsl.hpp"

#include <thread>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <functional>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;

struct ping {
  int32_t value;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, ping& x) {
  return f(meta::type_name("ping"), x.value);
}

bool operator==(const ping& lhs, const ping& rhs) {
  return lhs.value == rhs.value;
}

struct pong {
  int32_t value;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, pong& x) {
  return f(meta::type_name("pong"), x.value);
}

bool operator==(const pong& lhs, const pong& rhs) {
  return lhs.value == rhs.value;
}

using server_type = typed_actor<replies_to<ping>::with<pong>>;

using client_type = typed_actor<>;

server_type::behavior_type server() {
  return {
    [](const ping & p) -> pong {
      CAF_CHECK_EQUAL(p.value, 42);
      return pong{p.value};
    }
  };
}

void run_client(int argc, char** argv, uint16_t port) {
  actor_system_config cfg;
  cfg.load<io::middleman>()
     .add_message_type<ping>("ping")
     .add_message_type<pong>("pong")
     .parse(argc, argv);
  actor_system sys{cfg};
  // check whether invalid_argument is thrown
  // when trying to connect to get an untyped
  // handle to the server
  auto res = sys.middleman().remote_actor("127.0.0.1", port);
  CAF_REQUIRE(!res);
  CAF_MESSAGE(sys.render(res.error()));
  CAF_MESSAGE("connect to typed_remote_actor");
  auto serv = unbox(sys.middleman().remote_actor<server_type>("127.0.0.1",
                                                              port));
  auto f = make_function_view(serv);
  CAF_CHECK_EQUAL(f(ping{42}), pong{42});
  anon_send_exit(serv, exit_reason::user_shutdown);
}

void run_server(int argc, char** argv) {
  actor_system_config cfg;
  cfg.load<io::middleman>()
     .add_message_type<ping>("ping")
     .add_message_type<pong>("pong")
     .parse(argc, argv);
  actor_system sys{cfg};
  auto port = unbox(sys.middleman().publish(sys.spawn(server), 0, "127.0.0.1"));
  CAF_REQUIRE(port != 0);
  CAF_MESSAGE("running on port " << port << ", start client");
  std::thread child{[=] { run_client(argc, argv, port); }};
  child.join();
}

CAF_TEST(test_typed_remote_actor) {
  auto argc = test::engine::argc();
  auto argv = test::engine::argv();
  run_server(argc, argv);
}

