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

#define CAF_SUITE io_automatic_connection
#include "caf/test/unit_test.hpp"

#include <set>
#include <thread>
#include <vector>

#include "caf/test/io_dsl.hpp"

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/test_multiplexer.hpp"

using namespace caf;
using namespace caf::io;

using std::string;

using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;

using test_one_atom = atom_constant<atom("test_one")>;
using done_atom = atom_constant<atom("shutdown")>;

/*

  This test checks whether automatic connections work as expected
  by first connecting three nodes "in line". In step 2, we send a
  message across the line, forcing the nodes to build a mesh. In step 3,
  we disconnect the node that originally connected the other two and expect
  that the other two nodes communicate uninterrupted.

  1) Initial setup:

    Earth ---- Mars ---- Jupiter

  2) After Jupiter has send a message to Earth:

    Earth ---- Mars
       \       /
        \     /
         \   /
        Jupiter

  3) After Earth has received the message and disconnected Mars:

    Earth ---- Jupiter

*/

namespace {

constexpr uint16_t port_earth   = 12340;
constexpr uint16_t port_mars    = 12341;
constexpr uint16_t port_jupiter = 12342;

class config : public actor_system_config {
public:
  config(bool use_tcp = true) {
    load<caf::io::middleman, io::network::test_multiplexer>();
    set("scheduler.policy", caf::atom("testing"));
    set("middleman.detach-utility-actors", false);
    set("middleman.enable-automatic-connections", true);
    set("middleman.enable-tcp", use_tcp);
    set("middleman.enable-udp", !use_tcp);
  }
};

class simple_config : public actor_system_config {
public:
  simple_config(bool use_tcp = true) {
    load<caf::io::middleman>();
    set("middleman.enable-automatic-connections", true);
    set("middleman.enable-tcp", use_tcp);
    set("middleman.enable-udp", !use_tcp);
  }
};

class fixture {
public:

  fixture(bool use_tcp = true)
    : cfg_earth(use_tcp),
      cfg_mars(use_tcp),
      cfg_jupiter(use_tcp),
      earth(cfg_earth),
      mars(cfg_mars),
      jupiter(cfg_jupiter) {
    CAF_MESSAGE("Earth  : " << to_string(earth.node()));
    CAF_MESSAGE("Mars   : " << to_string(mars.node()));
    CAF_MESSAGE("Jupiter: " << to_string(jupiter.node()));
  }

  simple_config cfg_earth;
  simple_config cfg_mars;
  simple_config cfg_jupiter;
  actor_system earth{cfg_earth};
  actor_system mars{cfg_mars};
  actor_system jupiter{cfg_jupiter};
};

class fixture_udp : public fixture {
public:
  fixture_udp() : fixture(false) {
    // nop
  }
};

behavior actor_jupiter(event_based_actor* self, actor mars) {
  return {
    [=](test_one_atom) {
      CAF_MESSAGE("sending message from Jupiter to Mars");
      self->send(mars, test_one_atom::value, self);
    },
    [=](done_atom) {
      CAF_MESSAGE("Jupiter received message from Earth, shutting down");
      self->quit();
    }
  };
}

behavior actor_mars(event_based_actor* self, actor earth) {
  return {
    [=](done_atom) {
      CAF_MESSAGE("Mars received message from Earth, shutting down");
      self->quit();
    },
    [=](test_one_atom, actor jupiter) {
      CAF_MESSAGE("sending message from Mars to Earth");
      self->send(earth, test_one_atom::value, jupiter, self);
    }
  };
}

behavior actor_earth(event_based_actor* self) {
  return {
    [=](test_one_atom, actor jupiter, actor mars) {
      CAF_MESSAGE("message from Jupiter reached Earth, "
                  "replying and shutting down");
      self->send(mars, done_atom::value);
      self->send(jupiter, done_atom::value);
      self->quit();
    }
  };
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(autoconn_tcp_simple_test, fixture)

CAF_TEST(build_triangle_simple_tcp) {
  CAF_MESSAGE("setting up Earth");
  auto on_earth = earth.spawn(actor_earth);
  auto earth_port = earth.middleman().publish(on_earth, 0);
  CAF_REQUIRE(earth_port);
  CAF_MESSAGE("Earth reachable via " << *earth_port);
  CAF_MESSAGE("setting up Mars");
  auto from_earth = mars.middleman().remote_actor("localhost", *earth_port);
  CAF_REQUIRE(from_earth);
  auto on_mars = mars.spawn(actor_mars, *from_earth);
  auto mars_port = mars.middleman().publish(on_mars, 0);
  CAF_REQUIRE(mars_port);
  CAF_MESSAGE("Mars reachable via " << *mars_port);
  CAF_MESSAGE("setting up Jupiter");
  auto from_mars = jupiter.middleman().remote_actor("localhost", *mars_port);
  CAF_REQUIRE(from_mars);
  auto on_jupiter = jupiter.spawn(actor_jupiter, *from_mars);
  CAF_MESSAGE("forwarding an actor from Jupiter to Earth via Mars");
  anon_send(on_jupiter, test_one_atom::value);
  jupiter.await_all_actors_done();
  mars.await_all_actors_done();
  earth.await_all_actors_done();
}

CAF_TEST_FIXTURE_SCOPE_END()

CAF_TEST_FIXTURE_SCOPE(autoconn_tcp_test, belt_fixture_t<config>)

CAF_TEST(build_triangle_tcp) {
  CAF_MESSAGE("Earth  : " << to_string(earth.sys.node()));
  CAF_MESSAGE("Mars   : " << to_string(mars.sys.node()));
  CAF_MESSAGE("Jupiter: " << to_string(jupiter.sys.node()));

  CAF_MESSAGE("setting up Earth");
  auto on_earth = earth.sys.spawn(actor_earth);
//  scoped_actor on_earth{earth.sys};
  CAF_MESSAGE("run initialization code");
  exec_all();
  CAF_MESSAGE("prepare connection");
  prepare_connection(earth, mars, "earth", port_earth);
  CAF_MESSAGE("publish dummy on earth");
//  earth.publish(actor_cast<actor>(on_earth), port_earth);
  earth.publish(on_earth, port_earth);

  CAF_MESSAGE("setting up Mars");
  auto from_earth = mars.remote_actor("earth", port_earth);
  CAF_REQUIRE(from_earth);
  auto on_mars = mars.sys.spawn(actor_mars, from_earth);
  CAF_MESSAGE("run initialization code");
  exec_all();
  CAF_MESSAGE("prepare connection");
  prepare_connection(mars, jupiter, "mars", port_mars);
  CAF_MESSAGE("publish dummy on earth");
  mars.publish(on_mars, port_mars);

  CAF_MESSAGE("setting up Jupiter");
  auto from_mars = jupiter.remote_actor("mars", port_mars);
  CAF_REQUIRE(from_mars);
  auto on_jupiter = jupiter.sys.spawn(actor_jupiter, from_mars);

  // This handle will be created by the test multiplexer for the automatically
  // opened socket when automatic connections are enabled.
  auto hdl_jupiter = accept_handle::from_int(std::numeric_limits<int64_t>::max());
  // Perpare autmomatic connection between Jupiter and Earth,
  prepare_connection(jupiter, earth, "jupiter", port_jupiter, hdl_jupiter);
  // Add the address information for this test to the config server on Mars.
  auto mars_config_server = mars.sys.registry().get(atom("ConfigServ"));
  network::address_listing interfaces{
    {network::protocol::ipv4, std::vector<std::string>{"jupiter"}}
  };
  basp::routing_table::address_map addrs{
    {network::protocol::tcp, {port_jupiter, interfaces}}
  };
  anon_send(actor_cast<actor>(mars_config_server), put_atom::value,
            to_string(jupiter.sys.node()), make_message(addrs));

  CAF_MESSAGE("forwarding an actor from Jupiter to Earth via Mars.");
  anon_send(on_jupiter, test_one_atom::value);
  exec_all();
}

CAF_TEST(break_triangle_tcp) {
  // TODO: Implement the same test as above, but kill the intermediate node
  //       that helped establish the connection.
}

CAF_TEST_FIXTURE_SCOPE_END()
