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

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/io/network/interfaces.hpp"
#include "caf/io/network/test_multiplexer.hpp"

using namespace caf;
using namespace caf::io;

using std::string;

using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;

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

/*
std::thread run_prog(const char* arg, uint16_t port, bool use_asio) {
  return detail::run_sub_unit_test(invalid_actor,
                                   test::engine::path(),
                                   test::engine::max_runtime(),
                                   CAF_XSTR(CAF_SUITE),
                                   use_asio,
                                   {"--port=" + std::to_string(port), arg});
}

// we run the same code on all three nodes, a simple ping-pong client
struct testee_state {
  std::set<actor> buddies;
  uint16_t port = 0;
  const char* name = "testee";
};

behavior testee(stateful_actor<testee_state>* self) {
  return {
    [self](ping_atom, actor buddy, bool please_broadcast) -> message {
      if (please_broadcast)
        for (auto& x : self->state.buddies)
          if (x != buddy)
            send_as(buddy, x, ping_atom::value, buddy, false);
      self->state.buddies.emplace(std::move(buddy));
      return make_message(pong_atom::value, self);
    },
    [self](pong_atom, actor buddy) {
      self->state.buddies.emplace(std::move(buddy));
    },
    [self](put_atom, uint16_t new_port) {
      self->state.port = new_port;
    },
    [self](get_atom) {
      return self->state.port;
    }
  };
}

void run_earth(bool use_asio, bool as_server, uint16_t pub_port) {
  scoped_actor self{system};
  struct captain : hook {
  public:
    captain(actor parent) : parent_(std::move(parent)) {
      // nop
    }

    void new_connection_established_cb(const node_id& node) override {
      anon_send(parent_, put_atom::value, node);
      call_next<hook::new_connection_established>(node);
    }

    void new_remote_actor_cb(const actor_addr& addr) override {
      anon_send(parent_, put_atom::value, addr);
      call_next<hook::new_remote_actor>(addr);
    }

    void connection_lost_cb(const node_id& dest) override {
      anon_send(parent_, delete_atom::value, dest);
    }

  private:
    actor parent_;
  };
  middleman::instance()->add_hook<captain>(self);
  auto aut = system.spawn(testee);
  auto port = publish(aut, pub_port);
  CAF_MESSAGE("published testee at port " << port);
  std::thread mars_process;
  std::thread jupiter_process;
  // launch process for Mars
  if (!as_server) {
    CAF_MESSAGE("launch process for Mars");
    mars_process = run_prog("--mars", port, use_asio);
  }
  CAF_MESSAGE("wait for Mars to connect");
  node_id mars;
  self->receive(
    [&](put_atom, const node_id& nid) {
      mars = nid;
      CAF_MESSAGE(CAF_ARG(mars));
    }
  );
  actor_addr mars_addr;
  uint16_t mars_port;
  self->receive_while([&] { return mars_addr == invalid_actor_addr; })(
    [&](put_atom, const actor_addr& addr) {
      auto hdl = actor_cast<actor>(addr);
      self->request(hdl, sys_atom::value, get_atom::value, "info").then(
        [&](ok_atom, const string&, const actor_addr&, const string& name) {
          if (name != "testee")
            return;
          mars_addr = addr;
          CAF_MESSAGE(CAF_ARG(mars_addr));
          self->request(actor_cast<actor>(mars_addr), get_atom::value).then(
            [&](uint16_t mp) {
              CAF_MESSAGE("mars published its actor at port " << mp);
              mars_port = mp;
            }
          );
        }
      );
    }
  );
  // launch process for Jupiter
  if (!as_server) {
    CAF_MESSAGE("launch process for Jupiter");
    jupiter_process = run_prog("--jupiter", mars_port, use_asio);
  }
  CAF_MESSAGE("wait for Jupiter to connect");
  self->receive(
    [](put_atom, const node_id& jupiter) {
      CAF_MESSAGE(CAF_ARG(jupiter));
    }
  );
  actor_addr jupiter_addr;
  self->receive_while([&] { return jupiter_addr == invalid_actor_addr; })(
    [&](put_atom, const actor_addr& addr) {
      auto hdl = actor_cast<actor>(addr);
      self->request(hdl, sys_atom::value, get_atom::value, "info").then(
        [&](ok_atom, const string&, const actor_addr&, const string& name) {
          if (name != "testee")
            return;
          jupiter_addr = addr;
          CAF_MESSAGE(CAF_ARG(jupiter_addr));
        }
      );
    }
  );
  CAF_MESSAGE("shutdown Mars");
  anon_send_exit(mars_addr, exit_reason::kill);
  if (mars_process.joinable())
    mars_process.join();
  self->receive(
    [&](delete_atom, const node_id& nid) {
      CAF_CHECK(nid == mars);
    }
  );
  CAF_MESSAGE("check whether we still can talk to Jupiter");
  self->send(aut, ping_atom::value, self, true);
  std::set<actor_addr> found;
  int i = 0;
  self->receive_for(i, 2)(
    [&](pong_atom, const actor&) {
      found.emplace(self->current_sender());
    }
  );
  std::set<actor_addr> expected{aut.address(), jupiter_addr};
  CAF_CHECK(found == expected);
  CAF_MESSAGE("shutdown Jupiter");
  anon_send_exit(jupiter_addr, exit_reason::kill);
  if (jupiter_process.joinable())
    jupiter_process.join();
  anon_send_exit(aut, exit_reason::kill);
}

void run_mars(uint16_t port_to_earth, uint16_t pub_port) {
  auto aut = system.spawn(testee);
  auto port = publish(aut, pub_port);
  anon_send(aut, put_atom::value, port);
  CAF_MESSAGE("published testee at port " << port);
  auto earth = remote_actor("localhost", port_to_earth);
  send_as(aut, earth, ping_atom::value, aut, false);
}

void run_jupiter(uint16_t port_to_mars) {
  auto aut = system.spawn(testee);
  auto mars = remote_actor("localhost", port_to_mars);
  send_as(aut, mars, ping_atom::value, aut, true);
}
*/

CAF_TEST(triangle_setup) {
  // this unit test is temporarily disabled until problems
  // with OBS are sorted out or new actor_system API is in place
}

/*
CAF_TEST(triangle_setup) {
  uint16_t port = 0;
  uint16_t publish_port = 0;
  auto argv = test::engine::argv();
  auto argc = test::engine::argc();
  auto r = message_builder(argv, argv + argc).extract_opts({
    {"port,p", "port of remote side (when running mars or jupiter)", port},
    {"mars", "run mars"},
    {"jupiter", "run jupiter"},
    {"use-asio", "use ASIO network backend (if available)"},
    {"server,s", "run in server mode (don't run clients)", publish_port}
  });
  // check arguments
  bool is_mars = r.opts.count("mars") > 0;
  bool is_jupiter = r.opts.count("jupiter") > 0;
  bool has_port = r.opts.count("port") > 0;
  if (((is_mars || is_jupiter) && !has_port) || (is_mars && is_jupiter)) {
    CAF_ERROR("need a port when running Mars or Jupiter and cannot "
              "both at the same time");
    return;
  }
  // enable automatic connections
  anon_send(whereis(atom("ConfigServ")), put_atom::value,
            "middleman.enable-automatic-connections", make_message(true));
  auto use_asio = r.opts.count("use-asio") > 0;
# ifdef CAF_USE_ASIO
  if (use_asio) {
    CAF_MESSAGE("enable ASIO backend");
    set_middleman<network::asio_multiplexer>();
  }
# endif // CAF_USE_ASIO
  auto as_server = r.opts.count("server") > 0;
  if (is_mars)
    run_mars(port, publish_port);
  else if (is_jupiter)
    run_jupiter(port);
  else
    run_earth(use_asio, as_server, publish_port);
  await_all_actors_done();
  shutdown();
}
*/
