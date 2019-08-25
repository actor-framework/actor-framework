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

#define CAF_SUITE io_dynamic_remote_actor
#include "caf/test/dsl.hpp"

#include <vector>
#include <sstream>
#include <utility>
#include <algorithm>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;

namespace {

class config : public actor_system_config {
public:
  config() {
    load<io::middleman>();
    add_message_type<std::vector<int>>("std::vector<int>");
    if (auto err = parse(test::engine::argc(), test::engine::argv()))
      CAF_FAIL("failed to parse config: " << to_string(err));
  }
};

struct fixture {
  // State for the server.
  config server_side_config;
  actor_system server_side;
  io::middleman& server_side_mm;

  // State for the client.
  config client_side_config;
  actor_system client_side;
  io::middleman& client_side_mm;

  fixture()
      : server_side(server_side_config),
        server_side_mm(server_side.middleman()),
        client_side(client_side_config),
        client_side_mm(client_side.middleman()) {
    localhost = "127.0.0.1";
    localhost_uri = unbox(uri::from_string("tcp://127.0.0.1"));
  }

  uri make_uri(uint16_t port) {
    return unbox(uri::from_string("tcp://127.0.0.1:" + std::to_string(port)));
  }

  const char* localhost;
  uri localhost_uri;
};

behavior make_pong_behavior() {
  return {
    [](int val) -> int {
      ++val;
      CAF_MESSAGE("pong with " << val);
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

behavior make_sort_behavior() {
  return {
    [](std::vector<int>& vec) -> std::vector<int> {
      CAF_MESSAGE("sorter received: " << deep_to_string(vec));
      std::sort(vec.begin(), vec.end());
      CAF_MESSAGE("sorter sent: " << deep_to_string(vec));
      return std::move(vec);
    }
  };
}

behavior make_sort_requester_behavior(event_based_actor* self,
                                      const actor& sorter) {
  self->send(sorter, std::vector<int>{5, 4, 3, 2, 1});
  return {
    [=](const std::vector<int>& vec) {
      CAF_MESSAGE("sort requester received: " << deep_to_string(vec));
      std::vector<int> expected_vec{1, 2, 3, 4, 5};
      CAF_CHECK_EQUAL(vec, expected_vec);
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

CAF_TEST(identity_semantics) {
  // Server side.
  auto server = server_side.spawn(make_pong_behavior);
  auto port1 = unbox(server_side_mm.publish(server, 0, localhost));
  auto port2 = unbox(server_side_mm.publish(server, 0, localhost));
  auto port3 = unbox(server_side_mm.publish(server, localhost_uri));
  CAF_REQUIRE_NOT_EQUAL(port1, port2);
  CAF_REQUIRE_NOT_EQUAL(port1, port3);
  auto same_server_1 = unbox(server_side_mm.remote_actor(localhost, port2));
  auto same_server_2 = unbox(server_side_mm.remote_actor(make_uri(port2)));
  CAF_REQUIRE_EQUAL(server, same_server_1);
  CAF_REQUIRE_EQUAL(server, same_server_2);
  CAF_CHECK_EQUAL(same_server_1.node(), server_side.node());
  CAF_CHECK_EQUAL(same_server_2.node(), server_side.node());
  auto server1 = unbox(client_side_mm.remote_actor(localhost, port1));
  auto server2 = unbox(client_side_mm.remote_actor(localhost, port2));
  auto server3 = unbox(client_side_mm.remote_actor(make_uri(port2)));
  CAF_CHECK_EQUAL(server1, client_side_mm.remote_actor(localhost, port1));
  CAF_CHECK_EQUAL(server2, client_side_mm.remote_actor(localhost, port2));
  CAF_CHECK_EQUAL(server3, client_side_mm.remote_actor(make_uri(port2)));
  anon_send_exit(server, exit_reason::user_shutdown);
}

CAF_TEST(ping_pong) {
  // Server side.
  auto pong_orig = server_side.spawn(make_pong_behavior);
  auto port = unbox(server_side_mm.publish(pong_orig, 0, localhost));
  // Client side.
  auto pong = unbox(client_side_mm.remote_actor(localhost, port));
  client_side.spawn(make_ping_behavior, pong);
}

CAF_TEST(ping_pong uri API) {
  // Server side.
  auto pong_orig = server_side.spawn(make_pong_behavior);
  auto port = unbox(server_side_mm.publish(pong_orig, localhost_uri));
  // Client side.
  auto pong = unbox(client_side_mm.remote_actor(make_uri(port)));
  client_side.spawn(make_ping_behavior, pong);
}

CAF_TEST(custom_message_type) {
  // Server side.
  auto sorter_orig = server_side.spawn(make_sort_behavior);
  auto port = unbox(server_side_mm.publish(sorter_orig, 0, localhost));
  // Client side.
  auto sorter = unbox(client_side_mm.remote_actor(localhost, port));
  client_side.spawn(make_sort_requester_behavior, sorter);
}

CAF_TEST(remote_link) {
  // Server side.
  auto mirror_orig = server_side.spawn(fragile_mirror);
  auto port = unbox(server_side_mm.publish(mirror_orig, 0, localhost));
  // Client side.
  auto mirror = unbox(client_side_mm.remote_actor(localhost, port));
  auto linker = client_side.spawn(linking_actor, mirror);
  scoped_actor self{client_side};
  self->wait_for(linker);
  CAF_MESSAGE("linker exited");
  self->wait_for(mirror);
  CAF_MESSAGE("mirror exited");
}

CAF_TEST_FIXTURE_SCOPE_END()
