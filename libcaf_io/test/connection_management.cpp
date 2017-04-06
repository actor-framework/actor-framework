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

#include <string>
#include <numeric>
#include <fstream>
#include <iostream>
#include <iterator>

#define CAF_SUITE io_connection_management
#include "caf/test/io_dsl.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/network/test_multiplexer.hpp"

using std::cout;
using std::endl;
using std::string;

using namespace caf;
using namespace caf::io;

namespace {

using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;

class remoting_config : public actor_system_config {
public:
  remoting_config() {
    load<middleman, network::test_multiplexer>();
    middleman_detach_utility_actors = false;
  }
};

basp_broker* get_basp_broker(middleman& mm) {
  auto hdl = mm.named_broker<basp_broker>(atom("BASP"));
  return dynamic_cast<basp_broker*>(actor_cast<abstract_actor*>(hdl));
}

class node_state : public test_coordinator_fixture<remoting_config> {
public:
  middleman& mm;
  network::test_multiplexer& mpx;
  basp_broker* basp;
  connection_handle conn;
  accept_handle acc;
  node_state* peer = nullptr;

  node_state()
      : mm(sys.middleman()),
        mpx(dynamic_cast<network::test_multiplexer&>(mm.backend())),
        basp(get_basp_broker(mm)) {
    // nop
  }

  void publish(actor whom, uint16_t port) {
    auto ma = mm.actor_handle();
    scoped_actor self{sys};
    std::set<std::string> sigs;
    // Make sure no pending BASP broker messages are in the queue.
    mpx.flush_runnables();
    // Trigger middleman actor.
    self->send(ma, publish_atom::value, port,
               actor_cast<strong_actor_ptr>(std::move(whom)), std::move(sigs),
               "", false);
    // Wait for the message of the middleman actor.
    expect((atom_value, uint16_t, strong_actor_ptr, std::set<std::string>,
            std::string, bool),
           from(self).to(sys.middleman().actor_handle())
           .with(publish_atom::value, port, _, _, _, false));
    mpx.exec_runnable();
    // Fetch response.
    self->receive(
      [](uint16_t) {
        // nop
      },
      [&](error& err) {
        CAF_FAIL(sys.render(err));
      }
    );
  }

  actor remote_actor(std::string host, uint16_t port) {
    CAF_MESSAGE("remote actor: " << host << ":" << port);
    // both schedulers must be idle at this point
    CAF_REQUIRE(!sched.has_job());
    CAF_REQUIRE(!peer->sched.has_job());
    // get necessary handles
    auto ma = mm.actor_handle();
    scoped_actor self{sys};
    // make sure no pending BASP broker messages are in the queue
    mpx.flush_runnables();
    // trigger middleman actor
    self->send(ma, connect_atom::value, std::move(host), port);
    expect((atom_value, std::string, uint16_t),
           from(self).to(ma).with(connect_atom::value, _, port));
    CAF_MESSAGE("wait for the message of the middleman actor in BASP");
    mpx.exec_runnable();
    CAF_MESSAGE("tell peer to accept the connection");
    peer->mpx.accept_connection(peer->acc);
    CAF_MESSAGE("run handshake between the two BASP broker instances");
    while (sched.run_once() || peer->sched.run_once()
           || mpx.try_exec_runnable() || peer->mpx.try_exec_runnable()
           || mpx.read_data() || peer->mpx.read_data()) {
      // re-run until handhsake is fully completed
    }
    CAF_MESSAGE("fetch remote actor proxy");
    actor result;
    self->receive(
      [&](node_id&, strong_actor_ptr& ptr, std::set<std::string>&) {
        result = actor_cast<actor>(std::move(ptr));
      },
      [&](error& err) {
        CAF_FAIL(sys.render(err));
      }
    );
    return result;
  }
};

struct fixture {
  node_state earth;
  node_state mars;

  fixture() {
    // Connect the buffers of mars and earth to setup a pseudo-network.
    mars.peer = &earth;
    earth.peer = &mars;
    // Set up mars to be the host and earth to be the client.
    earth.conn = connection_handle::from_int(1);
    mars.conn = connection_handle::from_int(2);
    mars.acc = accept_handle::from_int(3);
    // Run any initialization code.
    exec_all();
  }

  // Convenience function for transmitting all "network" traffic.
  void network_traffic() {
    while (earth.mpx.try_exec_runnable() || mars.mpx.try_exec_runnable()
           || earth.mpx.read_data() || mars.mpx.read_data()) {
      // rince and repeat
    }
  }

  // Convenience function for transmitting all "network" traffic and running
  // all executables on earth and mars.
  void exec_all() {
    while (earth.mpx.try_exec_runnable() || mars.mpx.try_exec_runnable()
           || earth.mpx.read_data() || mars.mpx.read_data()
           || earth.sched.run_once() || mars.sched.run_once()) {
      // rince and repeat
    }
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(connection_management_tests, fixture)

CAF_TEST(reconnect) {
  auto server_impl = [](event_based_actor* self) -> behavior {
    return {
      [=](ping_atom) {
        CAF_MESSAGE("received ping message in client");
        self->monitor(self->current_sender());
        return pong_atom::value;
      }
    };
  };
  auto client_impl = [](event_based_actor* self) -> behavior {
    return {
      [=](const actor& server_ref) {
        auto mm = self->system().middleman().actor_handle();
        self->monitor(server_ref);
        self->request(server_ref, infinite, ping_atom::value).then(
          [=](pong_atom) {
            CAF_MESSAGE("received pong message in client");
          }
        );
      }
    };
  };
  CAF_MESSAGE("earth: " << to_string(earth.sys.node()));
  CAF_MESSAGE("mars: " << to_string(mars.sys.node()));
  auto client = earth.sys.spawn(client_impl);
  auto server = mars.sys.spawn(server_impl);
  exec_all();
  CAF_MESSAGE("prepare connections on earth and mars");
  mars.mpx.prepare_connection(mars.acc, mars.conn, earth.mpx, "mars", 8080,
                              earth.conn);
  CAF_MESSAGE("publish sink on mars");
  mars.publish(server, 8080);
  CAF_MESSAGE("connect from earth to mars");
  auto proxy = earth.remote_actor("mars", 8080);
  CAF_MESSAGE("got proxy: " << to_string(proxy) << ", send it to client");
  anon_send(client, proxy);
  expect_on(earth, (actor), from(_).to(client).with(proxy));
  network_traffic();
  expect_on(mars, (atom_value), from(_).to(server).with(ping_atom::value));
  network_traffic();
  expect_on(earth, (atom_value), from(_).to(client).with(pong_atom::value));
  CAF_MESSAGE("fake disconnect between mars and earth");
  earth.mpx.close(earth.conn);
  mars.mpx.close(mars.conn);
  network_traffic();
  CAF_MESSAGE("expect down messages in server and client");
  expect_on(earth, (down_msg), from(_).to(client).with(_));
  expect_on(mars, (down_msg), from(_).to(server).with(_));
  exec_all();
  CAF_MESSAGE("reconnect mars and earth");
  proxy = earth.remote_actor("mars", 8080);
  anon_send(client, proxy);
  expect_on(earth, (actor), from(_).to(client).with(proxy));
  network_traffic();
  expect_on(mars, (atom_value), from(_).to(server).with(ping_atom::value));
  network_traffic();
  expect_on(earth, (atom_value), from(_).to(client).with(pong_atom::value));
  anon_send_exit(client, exit_reason::user_shutdown);
  anon_send_exit(server, exit_reason::user_shutdown);
}

CAF_TEST_FIXTURE_SCOPE_END()
