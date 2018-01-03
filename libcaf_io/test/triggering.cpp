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

#define CAF_SUITE io_triggering
#include "caf/test/unit_test.hpp"

#include <memory>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;
using namespace caf::io;

namespace {

// -- client implementation, used for both test servers ------------------------

behavior client(broker* self, connection_handle hdl) {
  std::vector<char> buf;
  buf.resize(200);
  std::iota(buf.begin(), buf.end(), 0);
  self->write(hdl, buf.size(), buf.data());
  CAF_REQUIRE_EQUAL(self->wr_buf(hdl).size(), 200u);
  self->configure_read(hdl, receive_policy::at_least(1));
  self->flush(hdl);
  return {
    [=](const new_data_msg&) {
      CAF_FAIL("server unexpectedly sent data");
    },
    [=](const connection_closed_msg&) {
      self->quit();
    }
  };
}

// -- first test server --------------------------------------------------------

struct server1_state {
  size_t received = 0;
  connection_handle peer = invalid_connection_handle;
};

// consumes 5 more tokens, then waits for passivated message to shutdown
behavior server1_stage4(stateful_actor<server1_state, broker>* self) {
  CAF_MESSAGE("enter server stage 4");
  self->trigger(self->state.peer, 5);
  return {
    [=](const new_data_msg& dm) {
      CAF_REQUIRE_EQUAL(dm.buf.size(), 10u);
      self->state.received += 1;
    },
    [=](const connection_passivated_msg& cp) {
      CAF_REQUIRE_EQUAL(cp.handle, self->state.peer);
      CAF_REQUIRE_EQUAL(self->state.received, 15u);
      CAF_REQUIRE_NOT_EQUAL(self->state.peer, invalid_connection_handle);
      // delay new tokens to force MM to remove this broker from its loop
      CAF_MESSAGE("server is done");
      self->quit();
    }
  };
}

// consumes 5 more tokens, then waits for passivated message to send itself
// a message that generates 5 more (force MM to actually remove this broker
// from its event loop and then re-adding it)
behavior server1_stage3(stateful_actor<server1_state, broker>* self) {
  CAF_MESSAGE("enter server stage 3");
  self->trigger(self->state.peer, 5);
  return {
    [=](const new_data_msg& dm) {
      CAF_REQUIRE_EQUAL(dm.buf.size(), 10u);
      self->state.received += 1;
    },
    [=](const connection_passivated_msg& cp) {
      CAF_REQUIRE_EQUAL(cp.handle, self->state.peer);
      CAF_REQUIRE_EQUAL(self->state.received, 10u);
      CAF_REQUIRE_NOT_EQUAL(self->state.peer, invalid_connection_handle);
      // delay new tokens to force MM to remove this broker from its loop
      self->send(self, ok_atom::value);
    },
    [=](ok_atom) {
      self->become(server1_stage4(self));
    }
  };
}

// consumes 5 tokens, then waits for passivated message and generates 5 more
behavior server1_stage2(stateful_actor<server1_state, broker>* self) {
  CAF_MESSAGE("enter server stage 2");
  self->trigger(self->state.peer, 5);
  return {
    [=](const new_data_msg& dm) {
      CAF_REQUIRE_EQUAL(dm.buf.size(), 10u);
      self->state.received += 1;
    },
    [=](const connection_passivated_msg& cp) {
      CAF_REQUIRE_EQUAL(cp.handle, self->state.peer);
      CAF_REQUIRE_EQUAL(self->state.received, 5u);
      CAF_REQUIRE_NOT_EQUAL(self->state.peer, invalid_connection_handle);
      self->become(server1_stage3(self));
    }
  };
}

// waits for the connection to the client
behavior server1(stateful_actor<server1_state, broker>* self) {
  return {
    [=](const new_connection_msg& nc) {
      CAF_REQUIRE_EQUAL(self->state.peer, invalid_connection_handle);
      self->state.peer = nc.handle;
      self->configure_read(nc.handle, receive_policy::exactly(10));
      self->become(server1_stage2(self));
    }
  };
}

// -- second test server -------------------------------------------------------

struct server2_state {
  size_t accepted = 0;
};

// consumes 5 more tokens, then waits for passivated message to shutdown
behavior server2_stage4(stateful_actor<server2_state, broker>* self) {
  CAF_MESSAGE("enter server stage 4");
  return {
    [=](const new_connection_msg&) {
      self->state.accepted += 1;
    },
    [=](const acceptor_passivated_msg&) {
      CAF_REQUIRE_EQUAL(self->state.accepted, 16u);
      CAF_MESSAGE("server is done");
      self->quit();
    }
  };
}

// consumes 5 more tokens, then waits for passivated message to send itself
// a message that generates 5 more (force MM to actually remove this broker
// from its event loop and then re-adding it)
behavior server2_stage3(stateful_actor<server2_state, broker>* self) {
  CAF_MESSAGE("enter server stage 3");
  return {
    [=](const new_connection_msg&) {
      self->state.accepted += 1;
    },
    [=](const acceptor_passivated_msg& cp) {
      CAF_REQUIRE_EQUAL(self->state.accepted, 11u);
      // delay new tokens to force MM to remove this broker from its loop
      self->send(self, ok_atom::value, cp.handle);
    },
    [=](ok_atom, accept_handle hdl) {
      self->trigger(hdl, 5);
      self->become(server2_stage4(self));
    }
  };
}

// consumes 5 tokens, then waits for passivated message and generates 5 more
behavior server2_stage2(stateful_actor<server2_state, broker>* self) {
  CAF_MESSAGE("enter server stage 2");
  CAF_REQUIRE_EQUAL(self->state.accepted, 1u);
  return {
    [=](const new_connection_msg&) {
      self->state.accepted += 1;
    },
    [=](const acceptor_passivated_msg& cp) {
      CAF_REQUIRE_EQUAL(self->state.accepted, 6u);
      self->trigger(cp.handle, 5);
      self->become(server2_stage3(self));
    }
  };
}

// waits for the connection to the client
behavior server2(stateful_actor<server2_state, broker>* self) {
  return {
    [=](const new_connection_msg& nc) {
      self->state.accepted += 1;
      self->trigger(nc.source, 5);
      self->become(server2_stage2(self));
    }
  };
}

// -- config and fixture -------------------------------------------------------

struct config : actor_system_config {
  config() {
    auto argc = test::engine::argc();
    auto argv = test::engine::argv();
    load<io::middleman>();
    parse(argc, argv);
  }
};

struct fixture {
  config client_cfg;
  actor_system client_system;
  config server_cfg;
  actor_system server_system;

  fixture() : client_system(client_cfg), server_system(server_cfg) {
    // nop
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(trigger_tests, fixture)

CAF_TEST(trigger_connection) {
  CAF_MESSAGE("spawn server");
  uint16_t port = 0;
  auto serv = server_system.middleman().spawn_server(server1, port);
  CAF_REQUIRE(serv);
  CAF_REQUIRE_NOT_EQUAL(port, 0);
  CAF_MESSAGE("server spawned at port " << port);
  std::thread child{[&] {
    auto cl = client_system.middleman().spawn_client(client, "localhost", port);
    CAF_REQUIRE(cl);
  }};
  child.join();
}

CAF_TEST(trigger_acceptor) {
  CAF_MESSAGE("spawn server");
  uint16_t port = 0;
  auto serv = server_system.middleman().spawn_server(server2, port);
  CAF_REQUIRE(serv);
  CAF_REQUIRE_NOT_EQUAL(port, 0);
  CAF_MESSAGE("server spawned at port " << port);
  std::thread child{[&] {
    // 16 clients will succeed to connect
    for (int i = 0; i < 16; ++i) {
      auto cl = client_system.middleman().spawn_client(client,
                                                       "localhost", port);
      CAF_REQUIRE(cl);
    }
  }};
  child.join();
}

CAF_TEST_FIXTURE_SCOPE_END()
