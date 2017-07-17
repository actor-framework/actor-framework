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

template <class BaseFixture =
            test_coordinator_fixture<caf::actor_system_config>>
class test_node_fixture : public BaseFixture {
public:
  using super = BaseFixture;

  caf::io::middleman& mm;
  caf::io::network::test_multiplexer& mpx;
  caf::io::basp_broker* basp;
  caf::io::connection_handle conn;
  caf::io::accept_handle acc;
  test_node_fixture* peer = nullptr;

  test_node_fixture()
      : mm(this->sys.middleman()),
        mpx(dynamic_cast<caf::io::network::test_multiplexer&>(mm.backend())),
        basp(get_basp_broker()) {
    // nop
  }

  // Convenience function for transmitting all "network" traffic and running
  // all executables on this node.
  void exec_all() {
    while (mpx.try_exec_runnable() || mpx.read_data()
           || this->sched.try_run_once()) {
      // rince and repeat
    }
  }

  void publish(caf::actor whom, uint16_t port) {
    auto ma = mm.actor_handle();
    auto& sys = this->sys;
    auto& sched = this->sched;
    caf::scoped_actor self{sys};
    std::set<std::string> sigs;
    // Make sure no pending BASP broker messages are in the queue.
    mpx.flush_runnables();
    // Trigger middleman actor.
    self->send(ma, caf::publish_atom::value, port,
               caf::actor_cast<caf::strong_actor_ptr>(std::move(whom)),
               std::move(sigs), "", false);
    // Wait for the message of the middleman actor.
    expect((caf::atom_value, uint16_t, caf::strong_actor_ptr,
            std::set<std::string>, std::string, bool),
           from(self)
           .to(sys.middleman().actor_handle())
           .with(caf::publish_atom::value, port, _, _, _, false));
    mpx.exec_runnable();
    // Fetch response.
    self->receive(
      [](uint16_t) {
        // nop
      },
      [&](caf::error& err) {
        CAF_FAIL(sys.render(err));
      }
    );
  }

  caf::actor remote_actor(std::string host, uint16_t port) {
    CAF_MESSAGE("remote actor: " << host << ":" << port);
    auto& sys = this->sys;
    auto& sched = this->sched;
    // both schedulers must be idle at this point
    CAF_REQUIRE(!sched.has_job());
    CAF_REQUIRE(!peer->sched.has_job());
    // get necessary handles
    auto ma = mm.actor_handle();
    caf::scoped_actor self{sys};
    // make sure no pending BASP broker messages are in the queue
    mpx.flush_runnables();
    // trigger middleman actor
    self->send(ma, caf::connect_atom::value, std::move(host), port);
    expect((caf::atom_value, std::string, uint16_t),
           from(self).to(ma).with(caf::connect_atom::value, _, port));
    CAF_MESSAGE("wait for the message of the middleman actor in BASP");
    mpx.exec_runnable();
    CAF_MESSAGE("tell peer to accept the connection");
    peer->mpx.accept_connection(peer->acc);
    CAF_MESSAGE("run handshake between the two BASP broker instances");
    while (sched.try_run_once() || peer->sched.try_run_once()
           || mpx.try_exec_runnable() || peer->mpx.try_exec_runnable()
           || mpx.read_data() || peer->mpx.read_data()) {
      // re-run until handhsake is fully completed
    }
    CAF_MESSAGE("fetch remote actor proxy");
    caf::actor result;
    self->receive(
      [&](caf::node_id&, caf::strong_actor_ptr& ptr, std::set<std::string>&) {
        result = caf::actor_cast<caf::actor>(std::move(ptr));
      },
      [&](caf::error& err) {
        CAF_FAIL(sys.render(err));
      }
    );
    return result;
  }

private:
  caf::io::basp_broker* get_basp_broker() {
    auto hdl = mm.named_broker<caf::io::basp_broker>(caf::atom("BASP"));
    return dynamic_cast<caf::io::basp_broker*>(
      caf::actor_cast<caf::abstract_actor*>(hdl));
  }
};

/// Binds `test_coordinator_fixture<Config>` to `test_node_fixture`.
template <class Config = caf::actor_system_config>
using test_node_fixture_t = test_node_fixture<test_coordinator_fixture<Config>>;

/// A simple fixture that includes two nodes (`earth` and `mars`) that are
/// connected to each other.
template <class BaseFixture =
            test_coordinator_fixture<caf::actor_system_config>>
class point_to_point_fixture {
public:
  using planet_type = test_node_fixture<BaseFixture>;

  planet_type earth;
  planet_type mars;

  point_to_point_fixture() {
    mars.peer = &earth;
    earth.peer = &mars;
    earth.acc = caf::io::accept_handle::from_int(1);
    earth.conn = caf::io::connection_handle::from_int(2);
    mars.acc = caf::io::accept_handle::from_int(3);
    mars.conn = caf::io::connection_handle::from_int(4);
  }

  // Convenience function for transmitting all "network" traffic.
  void network_traffic() {
    run_exhaustively([](planet_type* x) {
      return x->mpx.try_exec_runnable() || x->mpx.read_data();
    });
  }

  // Convenience function for transmitting all "network" traffic and running
  // all executables on earth and mars.
  void exec_all() {
    run_exhaustively([](planet_type* x) {
      return x->mpx.try_exec_runnable() || x->mpx.read_data()
             || x->sched.try_run_once();
    });
  }

  void prepare_connection(planet_type& server, planet_type& client,
                          std::string host, uint16_t port) {
    server.mpx.prepare_connection(server.acc, server.conn, client.mpx,
                                  std::move(host), port, client.conn);
  }

private:
  template <class F>
  void run_exhaustively(F f) {
    planet_type* planets[] = {&earth, &mars};
    while (std::any_of(std::begin(planets), std::end(planets), f))
      ; // rince and repeat
  }
};

/// Binds `test_coordinator_fixture<Config>` to `point_to_point_fixture`.
template <class Config = caf::actor_system_config>
using point_to_point_fixture_t =
  point_to_point_fixture<test_coordinator_fixture<Config>>;

}// namespace <anonymous>

#define expect_on(where, types, fields)                                        \
  CAF_MESSAGE(#where << ": expect" << #types << "." << #fields);               \
  expect_clause< CAF_EXPAND(CAF_DSL_LIST types) >{where . sched} . fields

#define disallow_on(where, types, fields)                                      \
  CAF_MESSAGE(#where << ": disallow" << #types << "." << #fields);             \
  disallow_clause< CAF_EXPAND(CAF_DSL_LIST types) >{where . sched} . fields
