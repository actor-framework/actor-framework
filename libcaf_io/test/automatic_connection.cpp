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

using set_atom = atom_constant<atom("set")>;
using begin_atom = atom_constant<atom("begin")>;
using middle_atom = atom_constant<atom("middle")>;
using end_atom = atom_constant<atom("end")>;

using msg_atom = atom_constant<atom("msg")>;
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

// Used for the tests with the test backend.
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

class config_udp : public config {
public:
  config_udp() : config(false) {
    // nop
  }
};

// Used for the tests with the default multiplexer backend.
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

struct cache {
  actor tmp;
};

behavior test_actor(stateful_actor<cache>* self, std::string location,
                    bool quit_directly) {
  return {
    [=](set_atom, actor val) {
      self->state.tmp = val;
    },
    [=](begin_atom) {
      CAF_REQUIRE(self->state.tmp);
      CAF_MESSAGE("starting messaging on " << location);
      self->send(self->state.tmp, middle_atom::value, self);
    },
    [=](middle_atom, actor start) {
      CAF_REQUIRE(self->state.tmp);
      CAF_MESSAGE("forwaring message on " << location);
      self->send(self->state.tmp, end_atom::value, start, self);
    },
    [=](end_atom, actor start, actor middle) {
      CAF_MESSAGE("message arrived on " << location);
      if (quit_directly) {
        CAF_MESSAGE("telling other nodes to quit from " << location);
        self->send(start, done_atom::value);
        self->send(middle, done_atom::value);
        self->send(self, done_atom::value);
      } else {
        CAF_MESSAGE("telling intermediate node to quit from " << location);
        self->state.tmp = start;
        self->send(middle, done_atom::value);
      }
    },
    [=](msg_atom) {
      CAF_REQUIRE(self->state.tmp);
      CAF_MESSAGE("telling tmp actor to quit from " << location);
      self->send(self->state.tmp, done_atom::value);
      self->send(self, done_atom::value);
    },
    [=](done_atom) {
      CAF_MESSAGE("actor on " << location << " is quitting");
      self->quit();
    }
  };
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(autoconn_tcp_simple_test, fixture)

CAF_TEST(build_triangle_simple_tcp) {
  CAF_MESSAGE("setting up Earth");
  auto on_earth = earth.spawn(test_actor, "Earth", true);
  auto earth_port = earth.middleman().publish(on_earth, 0);
  CAF_REQUIRE(earth_port);
  CAF_MESSAGE("Earth reachable via " << *earth_port);
  CAF_MESSAGE("setting up Mars");
  auto from_earth = mars.middleman().remote_actor("localhost", *earth_port);
  CAF_REQUIRE(from_earth);
  auto on_mars = mars.spawn(test_actor, "Mars", true);
  anon_send(on_mars, set_atom::value, *from_earth);
  auto mars_port = mars.middleman().publish(on_mars, 0);
  CAF_REQUIRE(mars_port);
  CAF_MESSAGE("Mars reachable via " << *mars_port);
  CAF_MESSAGE("setting up Jupiter");
  auto from_mars = jupiter.middleman().remote_actor("localhost", *mars_port);
  CAF_REQUIRE(from_mars);
  auto on_jupiter = jupiter.spawn(test_actor, "Jupiter", true);
  anon_send(on_jupiter, set_atom::value, *from_mars);
  CAF_MESSAGE("forwarding an actor from Jupiter to Earth via Mars");
  anon_send(on_jupiter, begin_atom::value);
  jupiter.await_all_actors_done();
  mars.await_all_actors_done();
  earth.await_all_actors_done();
}

CAF_TEST(break_triangle_simple_tcp) {
  actor on_earth;
  actor on_jupiter;
  {
    simple_config conf;
    actor_system mars(conf);
    // Earth.
    CAF_MESSAGE("setting up Earth");
    on_earth = earth.spawn(test_actor, "Earth", false);
    auto earth_port = earth.middleman().publish(on_earth, 0);
    CAF_REQUIRE(earth_port);
    CAF_MESSAGE("Earth reachable via " << *earth_port);
    // Mars.
    CAF_MESSAGE("setting up Mars");
    auto from_earth = mars.middleman().remote_actor("localhost", *earth_port);
    CAF_REQUIRE(from_earth);
    auto on_mars = mars.spawn(test_actor, "Mars", false);
    anon_send(on_mars, set_atom::value, *from_earth);
    auto mars_port = mars.middleman().publish(on_mars, 0);
    CAF_REQUIRE(mars_port);
    CAF_MESSAGE("Mars reachable via " << *mars_port);
    // Jupiter.
    CAF_MESSAGE("setting up Jupiter");
    auto from_mars = jupiter.middleman().remote_actor("localhost", *mars_port);
    CAF_REQUIRE(from_mars);
    on_jupiter = jupiter.spawn(test_actor, "Jupiter", false);
    anon_send(on_jupiter, set_atom::value, *from_mars);
    // Trigger the connection setup.
    CAF_MESSAGE("forwarding an actor from Jupiter to Earth via Mars");
    anon_send(on_jupiter, begin_atom::value);
    mars.await_all_actors_done();
    // Leaving the scope will shutdown Mars.
  }
  // Let the remaining nodes communicate.
  anon_send(on_earth, msg_atom::value);
  jupiter.await_all_actors_done();
  earth.await_all_actors_done();
}

CAF_TEST_FIXTURE_SCOPE_END()

CAF_TEST_FIXTURE_SCOPE(autoconn_udp_simple_test, fixture_udp)

CAF_TEST_DISABLED(build_triangle_simple_udp) {
  CAF_MESSAGE("setting up Earth");
  auto on_earth = earth.spawn(test_actor, "Earth", true);
  auto earth_port = earth.middleman().publish_udp(on_earth, 0);
  CAF_REQUIRE(earth_port);
  CAF_MESSAGE("Earth reachable via " << *earth_port);
  CAF_MESSAGE("setting up Mars");
  auto from_earth = mars.middleman().remote_actor_udp("localhost", *earth_port);
  CAF_REQUIRE(from_earth);
  auto on_mars = mars.spawn(test_actor, "Mars", true);
  anon_send(on_mars, set_atom::value, *from_earth);
  auto mars_port = mars.middleman().publish_udp(on_mars, 0);
  CAF_REQUIRE(mars_port);
  CAF_MESSAGE("Mars reachable via " << *mars_port);
  CAF_MESSAGE("setting up Jupiter");
  auto from_mars = jupiter.middleman().remote_actor_udp("localhost", *mars_port);
  CAF_REQUIRE(from_mars);
  auto on_jupiter = jupiter.spawn(test_actor, "Jupiter", true);
  anon_send(on_jupiter, set_atom::value, *from_mars);
  CAF_MESSAGE("forwarding an actor from Jupiter to Earth via Mars");
  anon_send(on_jupiter, begin_atom::value);
  jupiter.await_all_actors_done();
  mars.await_all_actors_done();
  earth.await_all_actors_done();
}

CAF_TEST_DISABLED(break_triangle_simple_udp) {
  actor on_earth;
  actor on_jupiter;
  {
    // Use UDP instead of TCP.
    simple_config conf(false);
    actor_system mars(conf);
    // Earth.
    CAF_MESSAGE("setting up Earth");
    on_earth = earth.spawn(test_actor, "Earth", false);
    auto earth_port = earth.middleman().publish_udp(on_earth, 0);
    CAF_REQUIRE(earth_port);
    CAF_MESSAGE("Earth reachable via " << *earth_port);
    // Mars.
    CAF_MESSAGE("setting up Mars");
    auto from_earth = mars.middleman().remote_actor_udp("localhost", *earth_port);
    if (!from_earth) {
      CAF_MESSAGE("Failed to contact earth: " << mars.render(from_earth.error()));
    }
    CAF_REQUIRE(from_earth);
    auto on_mars = mars.spawn(test_actor, "Mars", false);
    anon_send(on_mars, set_atom::value, *from_earth);
    auto mars_port = mars.middleman().publish_udp(on_mars, 0);
    CAF_REQUIRE(mars_port);
    CAF_MESSAGE("Mars reachable via " << *mars_port);
    // Jupiter.
    CAF_MESSAGE("setting up Jupiter");
    auto from_mars = jupiter.middleman().remote_actor_udp("localhost", *mars_port);
    CAF_REQUIRE(from_mars);
    on_jupiter = jupiter.spawn(test_actor, "Jupiter", false);
    anon_send(on_jupiter, set_atom::value, *from_mars);
    // Trigger the connection setup.
    CAF_MESSAGE("forwarding an actor from Jupiter to Earth via Mars");
    anon_send(on_jupiter, begin_atom::value);
    mars.await_all_actors_done();
    // Leaving the scope will shutdown Mars.
  }
  // Let the remaining nodes communicate.
  anon_send(on_earth, msg_atom::value);
  jupiter.await_all_actors_done();
  earth.await_all_actors_done();
}

CAF_TEST_FIXTURE_SCOPE_END()

CAF_TEST_FIXTURE_SCOPE(autoconn_tcp_test, belt_fixture<>)

CAF_TEST_DISABLED(build_triangle_tcp) {
  CAF_MESSAGE("Earth  : " << to_string(earth.sys.node()));
  CAF_MESSAGE("Mars   : " << to_string(mars.sys.node()));
  CAF_MESSAGE("Jupiter: " << to_string(jupiter.sys.node()));
  // Earth.
  CAF_MESSAGE("setting up Earth");
  auto on_earth = earth.sys.spawn(test_actor, "Earth", true);
  CAF_MESSAGE("run initialization code");
  exec_all();
  CAF_MESSAGE("prepare connection");
  prepare_connection(earth, mars, "earth", port_earth);
  CAF_MESSAGE("publish dummy on earth");
  earth.publish(on_earth, port_earth);
  // Mars.
  CAF_MESSAGE("setting up Mars");
  auto from_earth = mars.remote_actor("earth", port_earth);
  CAF_REQUIRE(from_earth);
  auto on_mars = mars.sys.spawn(test_actor, "Mars", true);
  anon_send(on_mars, set_atom::value, from_earth);
  CAF_MESSAGE("run initialization code");
  exec_all();
  CAF_MESSAGE("prepare connection");
  prepare_connection(mars, jupiter, "mars", port_mars);
  CAF_MESSAGE("publish dummy on mars");
  mars.publish(on_mars, port_mars);
  // Jupiter
  CAF_MESSAGE("setting up Jupiter");
  auto from_mars = jupiter.remote_actor("mars", port_mars);
  CAF_REQUIRE(from_mars);
  auto on_jupiter = jupiter.sys.spawn(test_actor, "Jupiter", true);
  anon_send(on_jupiter, set_atom::value, from_mars);
  exec_all();
  // This handle will be created by the test multiplexer for the automatically
  // opened socket when automatic connections are enabled.
  auto hdl_jupiter = accept_handle::from_int(std::numeric_limits<int64_t>::max());
  // Prepare automatic connection between Jupiter and Earth.
  prepare_connection(jupiter, earth, "jupiter", port_jupiter, hdl_jupiter);
  // Add the address information for this test to the config server on Mars.
  auto mars_config_server = mars.sys.registry().get(atom("PeerServ"));
  network::address_listing interfaces{
    {network::protocol::ipv4, std::vector<std::string>{"jupiter"}}
  };
  basp::routing_table::address_map addrs{
    {network::protocol::tcp, {port_jupiter, interfaces}}
  };
  anon_send(actor_cast<actor>(mars_config_server), put_atom::value,
            to_string(jupiter.sys.node()), make_message(addrs));
  // Trigger the automatic connection setup.
  CAF_MESSAGE("forwarding an actor from Jupiter to Earth via Mars");
  anon_send(on_jupiter, begin_atom::value);
  exec_all();
}

/*

CAF_TEST(break_triangle_tcp)  {
  CAF_MESSAGE("Earth  : " << to_string(earth.sys.node()));
  CAF_MESSAGE("Mars   : " << to_string(mars.sys.node()));
  CAF_MESSAGE("Jupiter: " << to_string(jupiter.sys.node()));
  // Earth.
  CAF_MESSAGE("setting up Earth");
  auto on_earth = earth.sys.spawn(test_actor, "Earth", false);
  CAF_MESSAGE("run initialization code");
  exec_all();
  CAF_MESSAGE("prepare connection");
  prepare_connection(earth, mars, "earth", port_earth);
  CAF_MESSAGE("publish dummy on earth");
  earth.publish(on_earth, port_earth);
  // Mars.
  CAF_MESSAGE("setting up Mars");
  auto from_earth = mars.remote_actor("earth", port_earth);
  CAF_REQUIRE(from_earth);
  auto on_mars = mars.sys.spawn(test_actor, "Mars", false);
  anon_send(on_mars, set_atom::value, from_earth);
  CAF_MESSAGE("run initialization code");
  exec_all();
  CAF_MESSAGE("prepare connection");
  prepare_connection(mars, jupiter, "mars", port_mars);
  CAF_MESSAGE("publish dummy on mars");
  mars.publish(on_mars, port_mars);
  // Jupiter.
  CAF_MESSAGE("setting up Jupiter");
  auto from_mars = jupiter.remote_actor("mars", port_mars);
  CAF_REQUIRE(from_mars);
  auto on_jupiter = jupiter.sys.spawn(test_actor, "Jupiter", false);
  anon_send(on_jupiter, set_atom::value, from_mars);
  exec_all();
  // This handle will be created by the test multiplexer for the automatically
  // opened socket when automatic connections are enabled.
  auto hdl_jupiter = accept_handle::from_int(std::numeric_limits<int64_t>::max());
  // Prepare automatic connection between Jupiter and Earth.
  prepare_connection(jupiter, earth, "jupiter", port_jupiter, hdl_jupiter);
  // Add the address information for this test to the config server on Mars.
  auto mars_config_server = mars.sys.registry().get(atom("PeerServ"));
  network::address_listing interfaces{
    {network::protocol::ipv4, std::vector<std::string>{"jupiter"}}
  };
  basp::routing_table::address_map addrs{
    {network::protocol::tcp, {port_jupiter, interfaces}}
  };
  anon_send(actor_cast<actor>(mars_config_server), put_atom::value,
            to_string(jupiter.sys.node()), make_message(addrs));
  // Trigger the automatic connection setup between the edge nodes.
  CAF_MESSAGE("forwarding an actor from Jupiter to Earth via Mars");
  anon_send(on_jupiter, begin_atom::value);
  exec_all();
  // Shutdown the basp broker of the intermediate node.
  anon_send_exit(mars.basp, exit_reason::kill);
  exec_all();
  // Let the remaining nodes communicate.
  anon_send(on_earth, msg_atom::value);
  exec_all();
}

CAF_TEST_FIXTURE_SCOPE_END()

CAF_TEST_FIXTURE_SCOPE(autoconn_udp_test, belt_fixture_t<config_udp>)

CAF_TEST(build_triangle_udp) {
  CAF_MESSAGE("Earth  : " << to_string(earth.sys.node()));
  CAF_MESSAGE("Mars   : " << to_string(mars.sys.node()));
  CAF_MESSAGE("Jupiter: " << to_string(jupiter.sys.node()));

  // Earth.
  CAF_MESSAGE("setting up Earth");
  auto on_earth = earth.sys.spawn(test_actor, "Earth", true);
  CAF_MESSAGE("run initialization code");
  exec_all();
  CAF_MESSAGE("prepare endpoints");
  prepare_endpoints(earth, mars, "earth", port_earth);
  CAF_MESSAGE("publish_udp dummy on earth");
  earth.publish_udp(on_earth, port_earth);

  // Mars.
  CAF_MESSAGE("setting up Mars");
  auto from_earth = mars.remote_actor_udp("earth", port_earth);
  CAF_REQUIRE(from_earth);
  auto on_mars = mars.sys.spawn(test_actor, "Mars", true);
  anon_send(on_mars, set_atom::value, from_earth);
  CAF_MESSAGE("run initialization code");
  exec_all();
  CAF_MESSAGE("prepare endpoints");
  datagram_handle mj, jm;
  prepare_endpoints(mars, jupiter, "mars", port_mars);
  CAF_MESSAGE("publish_udp dummy on mars");
  mars.publish_udp(on_mars, port_mars);

  // Jupiter
  CAF_MESSAGE("setting up Jupiter");
  auto from_mars = jupiter.remote_actor_udp("mars", port_mars);
  CAF_REQUIRE(from_mars);
  auto on_jupiter = jupiter.sys.spawn(test_actor, "Jupiter", true);
  anon_send(on_jupiter, set_atom::value, from_mars);
  exec_all();

  // This handle will be created by the test multiplexer for the automatically
  // opened socket when automatic connections are enabled.
  auto hdl_jup = datagram_handle::from_int(std::numeric_limits<int64_t>::max());
  // Prepare automatic connection between Jupiter and Earth.
  datagram_handle je, ej;
  prepare_endpoints(jupiter, earth, "jupiter", port_jupiter, hdl_jup);
  // Add the address information for this test to the config server on Mars.
  auto mars_config_server = mars.sys.registry().get(atom("PeerServ"));
  network::address_listing interfaces{
    {network::protocol::ipv4, std::vector<std::string>{"jupiter"}}
  };
  basp::routing_table::address_map addrs{
    {network::protocol::udp, {port_jupiter, interfaces}}
  };
  anon_send(actor_cast<actor>(mars_config_server), put_atom::value,
            to_string(jupiter.sys.node()), make_message(addrs));
  // Trigger the automatic connection setup.
  CAF_MESSAGE("forwarding an actor from Jupiter to Earth via Mars");
  anon_send(on_jupiter, begin_atom::value);
  exec_all();
}

CAF_TEST(break_triangle_udp)  {
  CAF_MESSAGE("Earth  : " << to_string(earth.sys.node()));
  CAF_MESSAGE("Mars   : " << to_string(mars.sys.node()));
  CAF_MESSAGE("Jupiter: " << to_string(jupiter.sys.node()));
  // Earth.
  CAF_MESSAGE("setting up Earth");
  auto on_earth = earth.sys.spawn(test_actor, "Earth", false);
  CAF_MESSAGE("run initialization code");
  exec_all();
  CAF_MESSAGE("prepare endpoints");
  prepare_endpoints(earth, mars, "earth", port_earth);
  CAF_MESSAGE("publish_udp dummy on earth");
  earth.publish_udp(on_earth, port_earth);
  // Mars.
  CAF_MESSAGE("setting up Mars");
  auto from_earth = mars.remote_actor_udp("earth", port_earth);
  CAF_REQUIRE(from_earth);
  auto on_mars = mars.sys.spawn(test_actor, "Mars", false);
  anon_send(on_mars, set_atom::value, from_earth);
  CAF_MESSAGE("run initialization code");
  exec_all();
  CAF_MESSAGE("prepare endpoints");
  prepare_endpoints(mars, jupiter, "mars", port_mars);
  CAF_MESSAGE("publish_udp dummy on mars");
  mars.publish_udp(on_mars, port_mars);
  // Jupiter.
  CAF_MESSAGE("setting up Jupiter");
  auto from_mars = jupiter.remote_actor_udp("mars", port_mars);
  CAF_REQUIRE(from_mars);
  auto on_jupiter = jupiter.sys.spawn(test_actor, "Jupiter", false);
  anon_send(on_jupiter, set_atom::value, from_mars);
  exec_all();
  // This handle will be created by the test multiplexer for the automatically
  // opened socket when automatic connections are enabled.
  auto hdl_jup = datagram_handle::from_int(std::numeric_limits<int64_t>::max());
  // Prepare automatic connection between Jupiter and Earth.
  prepare_endpoints(jupiter, earth, "jupiter", port_jupiter, hdl_jup);
  // Add the address information for this test to the config server on Mars.
  auto mars_config_server = mars.sys.registry().get(atom("PeerServ"));
  network::address_listing interfaces{
    {network::protocol::ipv4, std::vector<std::string>{"jupiter"}}
  };
  basp::routing_table::address_map addrs{
    {network::protocol::udp, {port_jupiter, interfaces}}
  };
  anon_send(actor_cast<actor>(mars_config_server), put_atom::value,
            to_string(jupiter.sys.node()), make_message(addrs));
  // Trigger the automatic connection setup between the edge nodes.
  CAF_MESSAGE("forwarding an actor from Jupiter to Earth via Mars");
  anon_send(on_jupiter, begin_atom::value);
  exec_all();
  // Shutdown the basp broker of the intermediate node.
  anon_send_exit(mars.basp, exit_reason::kill);
  exec_all();
  // Let the remaining nodes communicate.
  anon_send(on_earth, msg_atom::value);
  exec_all();
}

*/

CAF_TEST_FIXTURE_SCOPE_END()
