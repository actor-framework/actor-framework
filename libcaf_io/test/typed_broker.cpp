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

#define CAF_SUITE io_typed_broker
#include "caf/test/dsl.hpp"

#include <memory>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/string_algorithms.hpp"

using namespace std;
using namespace caf;
using namespace caf::io;

namespace {

using publish_atom = atom_constant<atom("publish")>;
using ping_atom = caf::atom_constant<atom("ping")>;
using pong_atom = caf::atom_constant<atom("pong")>;
using kickoff_atom = caf::atom_constant<atom("kickoff")>;

using peer = connection_handler::extend<reacts_to<ping_atom, int>,
                                        reacts_to<pong_atom, int>>;

using acceptor = accept_handler::extend<replies_to<publish_atom>::with<uint16_t>>;

using ping_actor = typed_actor<replies_to<pong_atom, int>::with<ping_atom, int>>;

using pong_actor = typed_actor<replies_to<ping_atom, int>::with<pong_atom, int>>;

behavior ping(event_based_actor* self, size_t num_pings) {
  CAF_MESSAGE("num_pings: " << num_pings);
  auto count = std::make_shared<size_t>(0);
  return {
    [=](kickoff_atom, const peer& pong) {
      CAF_MESSAGE("received `kickoff_atom`");
      self->send(pong, ping_atom::value, 1);
      self->become(
        [=](pong_atom, int value) -> std::tuple<ping_atom, int> {
          if (++*count >= num_pings) {
            CAF_MESSAGE("received " << num_pings
                        << " pings, call self->quit");
            self->quit();
          }
          return std::make_tuple(ping_atom::value, value + 1);
        }
      );
    }
  };
}

behavior pong(event_based_actor* self) {
  CAF_MESSAGE("pong actor started");
  self->set_down_handler([=](down_msg& dm) {
    CAF_MESSAGE("received: " << to_string(dm.reason));
    self->quit(dm.reason);
  });
  return {
    [=](ping_atom, int value) -> std::tuple<atom_value, int> {
      CAF_MESSAGE("received: 'ping', " << value);
      self->monitor(self->current_sender());
      // set next behavior
      self->become(
        [](ping_atom, int val) {
          //CAF_MESSAGE("received: 'ping', " << val);
          return std::make_tuple(pong_atom::value, val);
        }
      );
      // reply to 'ping'
      return std::make_tuple(pong_atom::value, value);
    }
  };
}

peer::behavior_type peer_fun(peer::broker_pointer self, connection_handle hdl,
                             const actor& buddy) {
  CAF_MESSAGE("peer_fun called");
  self->monitor(buddy);
  // assume exactly one connection
  CAF_REQUIRE_EQUAL(self->connections().size(), 1u);
  self->configure_read(
    hdl, receive_policy::exactly(sizeof(atom_value) + sizeof(int)));
  auto write = [=](atom_value x, int y) {
    auto& buf = self->wr_buf(hdl);
    binary_serializer sink{self->system(), buf};
    auto e = sink(x, y);
    CAF_REQUIRE(!e);
    self->flush(hdl);
  };
  self->set_down_handler([=](down_msg& dm) {
    CAF_MESSAGE("received down_msg");
    if (dm.source == buddy)
      self->quit(std::move(dm.reason));
  });
  return {
    [=](const connection_closed_msg&) {
      CAF_MESSAGE("received connection_closed_msg");
      self->quit();
    },
    [=](const new_data_msg& msg) {
      CAF_MESSAGE("received new_data_msg");
      atom_value x;
      int y;
      binary_deserializer source{self->system(), msg.buf};
      auto e = source(x, y);
      CAF_REQUIRE(!e);
      if (x == pong_atom::value)
        self->send(actor_cast<ping_actor>(buddy), pong_atom::value, y);
      else
        self->send(actor_cast<pong_actor>(buddy), ping_atom::value, y);
    },
    [=](ping_atom, int value) {
      CAF_MESSAGE("received: 'ping', " << value);
      write(ping_atom::value, value);
    },
    [=](pong_atom, int value) {
      CAF_MESSAGE("received: 'pong', " << value);
      write(pong_atom::value, value);
    }
  };
}

acceptor::behavior_type acceptor_fun(acceptor::broker_pointer self,
                                     const actor& buddy) {
  CAF_MESSAGE("peer_acceptor_fun");
  return {
    [=](const new_connection_msg& msg) {
      CAF_MESSAGE("received `new_connection_msg`");
      self->fork(peer_fun, msg.handle, buddy);
      self->quit();
    },
    [](const acceptor_closed_msg&) {
      // nop
    },
    [=](publish_atom) -> expected<uint16_t> {
      auto dm = self->add_tcp_doorman(0, "127.0.0.1");
      if (dm)
        return get<1>(*dm);
      return std::move(dm.error());
    }
  };
}

void run_client(int argc, char** argv, uint16_t port) {
  actor_system_config cfg;
  cfg.load<io::middleman>();
  if (auto err = cfg.parse(argc, argv))
    CAF_FAIL("failed to parse config: " << to_string(err));
  actor_system system{cfg};
  auto p = system.spawn(ping, size_t{10});
  CAF_MESSAGE("spawn_client_typed...");
  auto cl = unbox(system.middleman().spawn_client(peer_fun,
                                                  "localhost", port, p));
  CAF_MESSAGE("spawn_client_typed finished");
  anon_send(p, kickoff_atom::value, cl);
  CAF_MESSAGE("`kickoff_atom` has been send");
}

void run_server(int argc, char** argv) {
  actor_system_config cfg;
  cfg.load<io::middleman>();
  if (auto err = cfg.parse(argc, argv))
    CAF_FAIL("failed to parse config: " << to_string(err));
  actor_system system{cfg};
  scoped_actor self{system};
  auto serv = system.middleman().spawn_broker(acceptor_fun, system.spawn(pong));
  std::thread child;
  self->request(serv, infinite, publish_atom::value).receive(
    [&](uint16_t port) {
      CAF_MESSAGE("server is running on port " << port);
      child = std::thread([=] {
        run_client(argc, argv, port);
      });
    },
    [&](error& err) {
      CAF_FAIL("error: " << system.render(err));
    }
  );
  self->await_all_other_actors_done();
  CAF_MESSAGE("wait for client system");
  child.join();
}

} // namespace <anonymous>

CAF_TEST(test_typed_broker) {
  run_server(test::engine::argc(), test::engine::argv());
}
