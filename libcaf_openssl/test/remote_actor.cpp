/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#define CAF_SUITE openssl_dynamic_remote_actor
#include "caf/test/unit_test.hpp"

#include <vector>
#include <sstream>
#include <utility>
#include <algorithm>

#include "caf/all.hpp"
#include "caf/io/all.hpp"
#include "caf/openssl/all.hpp"

using namespace caf;

namespace {

constexpr char local_host[] = "127.0.0.1";

class config : public actor_system_config {
public:
  config() {
    load<io::middleman>();
    load<openssl::manager>();
    add_message_type<std::vector<int>>("std::vector<int>");
    actor_system_config::parse(test::engine::argc(),
                               test::engine::argv());
  }
};

struct fixture {
  config server_side_config;
  actor_system server_side{server_side_config};
  config client_side_config;
  actor_system client_side{client_side_config};
};

behavior make_pong_behavior() {
  return {
    [](int val) -> int {
      CAF_MESSAGE("pong with " << ++val);
      return val;
    }
  };
}

behavior make_ping_behavior(event_based_actor* self, const actor& pong) {
  CAF_MESSAGE("ping with " << 0);
  self->send(pong, 0);
  return {
    [=](int val) -> int {
      if (val == 3) {
        CAF_MESSAGE("ping with exit");
        self->send_exit(self->current_sender(),
                        exit_reason::user_shutdown);
        CAF_MESSAGE("ping quits");
        self->quit();
      }
      CAF_MESSAGE("ping with " << val);
      return val;
    }
  };
}

std::string to_string(const std::vector<int>& vec) {
  std::ostringstream os;
  for (size_t i = 0; i + 1 < vec.size(); ++i)
    os << vec[i] << ", ";
  os << vec.back();
  return os.str();
}

behavior make_sort_behavior() {
  return {
    [](std::vector<int>& vec) -> std::vector<int> {
      CAF_MESSAGE("sorter received: " << to_string(vec));
      std::sort(vec.begin(), vec.end());
      CAF_MESSAGE("sorter sent: " << to_string(vec));
      return std::move(vec);
    }
  };
}

behavior make_sort_requester_behavior(event_based_actor* self, const actor& sorter) {
  self->send(sorter, std::vector<int>{5, 4, 3, 2, 1});
  return {
    [=](const std::vector<int>& vec) {
      CAF_MESSAGE("sort requester received: " << to_string(vec));
      for (size_t i = 1; i < vec.size(); ++i)
        CAF_CHECK_EQUAL(static_cast<int>(i), vec[i - 1]);
      self->send_exit(sorter, exit_reason::user_shutdown);
      self->quit();
    }
  };
}

behavior fragile_mirror(event_based_actor* self) {
  return {
    [=](int i) {
      self->quit(exit_reason::user_shutdown);
      return i;
    }
  };
}

behavior linking_actor(event_based_actor* self, const actor& buddy) {
  CAF_MESSAGE("link to mirror and send dummy message");
  self->link_to(buddy);
  self->send(buddy, 42);
  return {
    [](int i) {
      CAF_CHECK_EQUAL(i, 42);
    }
  };
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(dynamic_remote_actor_tests, fixture)

using openssl::remote_actor;
using openssl::publish;

CAF_TEST(identity_semantics) {
  // server side
  auto server = server_side.spawn(make_pong_behavior);
  CAF_EXP_THROW(port1, publish(server, 0, local_host));
  CAF_EXP_THROW(port2, publish(server, 0, local_host));
  CAF_REQUIRE_NOT_EQUAL(port1, port2);
  CAF_EXP_THROW(same_server, remote_actor(server_side, local_host, port2));
  CAF_REQUIRE_EQUAL(same_server, server);
  CAF_CHECK_EQUAL(same_server->node(), server_side.node());
  CAF_EXP_THROW(server1, remote_actor(client_side, local_host, port1));
  CAF_EXP_THROW(server2, remote_actor(client_side, local_host, port2));
  CAF_CHECK_EQUAL(server1, remote_actor(client_side, local_host, port1));
  CAF_CHECK_EQUAL(server2, remote_actor(client_side, local_host, port2));
  anon_send_exit(server, exit_reason::user_shutdown);
}

CAF_TEST(ping_pong) {
  // server side
  CAF_EXP_THROW(port,
                publish(server_side.spawn(make_pong_behavior), 0, local_host));
  // client side
  CAF_EXP_THROW(pong, remote_actor(client_side, local_host, port));
  client_side.spawn(make_ping_behavior, pong);
}

CAF_TEST(custom_message_type) {
  // server side
  CAF_EXP_THROW(port,
                publish(server_side.spawn(make_sort_behavior), 0, local_host));
  // client side
  CAF_EXP_THROW(sorter, remote_actor(client_side, local_host, port));
  client_side.spawn(make_sort_requester_behavior, sorter);
}

CAF_TEST(remote_link) {
  // server side
  CAF_EXP_THROW(port,
                publish(server_side.spawn(fragile_mirror), 0, local_host));
  // client side
  CAF_EXP_THROW(mirror, remote_actor(client_side, local_host, port));
  auto linker = client_side.spawn(linking_actor, mirror);
  scoped_actor self{client_side};
  self->wait_for(linker);
  CAF_MESSAGE("linker exited");
  self->wait_for(mirror);
  CAF_MESSAGE("mirror exited");
}

CAF_TEST_FIXTURE_SCOPE_END()
