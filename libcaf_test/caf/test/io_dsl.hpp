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

#include "caf/io/all.hpp"
#include "caf/io/network/test_multiplexer.hpp"

#include "caf/test/dsl.hpp"

namespace {

using namespace caf;
using namespace caf::io;

template <class Config = caf::actor_system_config>
class test_node_fixture : public test_coordinator_fixture<Config> {
public:
  using super = test_coordinator_fixture<Config>;

  middleman& mm;
  network::test_multiplexer& mpx;
  basp_broker* basp;
  connection_handle conn;
  accept_handle acc;
  test_node_fixture* peer = nullptr;
  strong_actor_ptr stream_serv;

  test_node_fixture()
      : mm(this->sys.middleman()),
        mpx(dynamic_cast<network::test_multiplexer&>(mm.backend())),
        basp(get_basp_broker()),
        stream_serv(this->sys.stream_serv()) {
    // nop
  }

  void publish(actor whom, uint16_t port) {
    auto ma = mm.actor_handle();
    auto& sys = this->sys;
    auto& sched = this->sched;
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
    auto& sys = this->sys;
    auto& sched = this->sched;
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

private:
  basp_broker* get_basp_broker() {
    auto hdl = mm.named_broker<basp_broker>(atom("BASP"));
    return dynamic_cast<basp_broker*>(actor_cast<abstract_actor*>(hdl));
  }
};

/// A simple fixture that includes two nodes (`earth` and `mars`) that are
/// connected to each other.
template <class Config = caf::actor_system_config>
class point_to_point_fixture {
public:
  using planet_type = test_node_fixture<Config>;

  planet_type earth;
  planet_type mars;

  point_to_point_fixture() {
    mars.peer = &earth;
    earth.peer = &mars;
    earth.acc = accept_handle::from_int(1);
    earth.conn = connection_handle::from_int(2);
    mars.acc = accept_handle::from_int(3);
    mars.conn = connection_handle::from_int(4);
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

  void prepare_connection(planet_type& server, planet_type& client,
                          std::string host, uint16_t port) {
    server.mpx.prepare_connection(server.acc, server.conn, client.mpx,
                                  std::move(host), port, client.conn);
  }
};

}// namespace <anonymous>

#define expect_on(where, types, fields)                                        \
  CAF_MESSAGE(#where << ": expect" << #types << "." << #fields);               \
  expect_clause< CAF_EXPAND(CAF_DSL_LIST types) >{where . sched} . fields

#define disallow_on(where, types, fields)                                      \
  CAF_MESSAGE(#where << ": disallow" << #types << "." << #fields);             \
  disallow_clause< CAF_EXPAND(CAF_DSL_LIST types) >{where . sched} . fields
