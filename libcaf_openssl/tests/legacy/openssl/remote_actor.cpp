// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE openssl.dynamic_remote_actor

#include "caf/io/all.hpp"

#include "caf/openssl/all.hpp"

#include "caf/all.hpp"

#include "openssl-test.hpp"
#include <signal.h>

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

using namespace caf;

namespace {

constexpr char local_host[] = "127.0.0.1";

class config : public actor_system_config {
public:
  config() {
    load<io::middleman>();
    load<openssl::manager>();
    // Setting the "max consecutive reads" to 1 is highly likely to cause
    // OpenSSL to buffer data internally and report "pending" data after a read
    // operation. This will trigger `must_read_more` in the SSL read policy
    // with high probability.
    set("caf.middleman.max-consecutive-reads", 1);
  }
};

struct fixture {
  config server_side_config;
  actor_system server_side{server_side_config};
  config client_side_config;
  actor_system client_side{client_side_config};
  fixture() {
#ifdef CAF_LINUX
    signal(SIGPIPE, SIG_IGN);
#endif
  }
};

behavior make_pong_behavior() {
  return {
    [](int val) -> int {
      ++val;
      MESSAGE("pong with " << val);
      return val;
    },
  };
}

behavior make_ping_behavior(event_based_actor* self, const actor& pong) {
  MESSAGE("ping with " << 0);
  self->send(pong, 0);
  return {
    [=](int val) -> int {
      if (val == 3) {
        MESSAGE("ping with exit");
        self->send_exit(self->current_sender(), exit_reason::user_shutdown);
        MESSAGE("ping quits");
        self->quit();
      }
      MESSAGE("ping with " << val);
      return val;
    },
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
      MESSAGE("sorter received: " << to_string(vec));
      std::sort(vec.begin(), vec.end());
      MESSAGE("sorter sent: " << to_string(vec));
      return std::move(vec);
    },
  };
}

behavior make_sort_requester_behavior(event_based_actor* self,
                                      const actor& sorter) {
  self->send(sorter, std::vector<int>{5, 4, 3, 2, 1});
  return {
    [=](const std::vector<int>& vec) {
      MESSAGE("sort requester received: " << to_string(vec));
      for (size_t i = 1; i < vec.size(); ++i)
        CHECK_EQ(static_cast<int>(i), vec[i - 1]);
      self->send_exit(sorter, exit_reason::user_shutdown);
      self->quit();
    },
  };
}

behavior fragile_mirror(event_based_actor* self) {
  return {
    [=](int i) {
      self->quit(exit_reason::user_shutdown);
      return i;
    },
  };
}

behavior linking_actor(event_based_actor* self, const actor& buddy) {
  MESSAGE("link to mirror and send dummy message");
  self->link_to(buddy);
  self->send(buddy, 42);
  return {
    [](int i) { CHECK_EQ(i, 42); },
  };
}

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

using openssl::publish;
using openssl::remote_actor;

CAF_TEST_DISABLED(identity_semantics) {
  // server side
  auto server = server_side.spawn(make_pong_behavior);
  auto port1 = unbox(publish(server, 0, local_host));
  auto port2 = unbox(publish(server, 0, local_host));
  CAF_REQUIRE_NOT_EQUAL(port1, port2);
  auto same_server = unbox(remote_actor(server_side, local_host, port2));
  CAF_REQUIRE_EQUAL(same_server, server);
  CHECK_EQ(same_server->node(), server_side.node());
  auto server1 = unbox(remote_actor(client_side, local_host, port1));
  auto server2 = unbox(remote_actor(client_side, local_host, port2));
  CHECK_EQ(server1, remote_actor(client_side, local_host, port1));
  CHECK_EQ(server2, remote_actor(client_side, local_host, port2));
  anon_send_exit(server, exit_reason::user_shutdown);
}

CAF_TEST_DISABLED(ping_pong) {
  // server side
  auto port
    = unbox(publish(server_side.spawn(make_pong_behavior), 0, local_host));
  // client side
  auto pong = unbox(remote_actor(client_side, local_host, port));
  client_side.spawn(make_ping_behavior, pong);
}

CAF_TEST_DISABLED(custom_message_type) {
  // server side
  auto port
    = unbox(publish(server_side.spawn(make_sort_behavior), 0, local_host));
  // client side
  auto sorter = unbox(remote_actor(client_side, local_host, port));
  client_side.spawn(make_sort_requester_behavior, sorter);
}

CAF_TEST_DISABLED(remote_link) {
  // server side
  auto port = unbox(publish(server_side.spawn(fragile_mirror), 0, local_host));
  // client side
  auto mirror = unbox(remote_actor(client_side, local_host, port));
  auto linker = client_side.spawn(linking_actor, mirror);
  scoped_actor self{client_side};
  self->wait_for(linker);
  MESSAGE("linker exited");
  self->wait_for(mirror);
  MESSAGE("mirror exited");
}

END_FIXTURE_SCOPE()
