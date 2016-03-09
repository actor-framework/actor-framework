/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include "caf/config.hpp"

#define CAF_SUITE io_typed_broker
#include "caf/test/unit_test.hpp"

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

using acceptor =
  accept_handler::extend<replies_to<publish_atom>::with<uint16_t>>;

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
      },
      others >> [=] {
        CAF_ERROR("Unexpected message");
      });
    },
    others >> [=] {
      CAF_ERROR("Unexpected message");
    }
  };
}

behavior pong(event_based_actor* self) {
  CAF_MESSAGE("pong actor started");
  return {
    [=](ping_atom, int value) -> std::tuple<atom_value, int> {
      CAF_MESSAGE("received `ping_atom`");
      self->monitor(self->current_sender());
      // set next behavior
      self->become(
        [](ping_atom, int val) {
          return std::make_tuple(pong_atom::value, val);
        },
        [=](const down_msg& dm) {
          CAF_MESSAGE("received down_msg{" << to_string(dm.reason) << "}");
          self->quit(dm.reason);
        },
        others >> [=] {
          CAF_ERROR("Unexpected message");
        }
      );
      // reply to 'ping'
      return std::make_tuple(pong_atom::value, value);
    },
    others >> [=] {
      CAF_ERROR("Unexpected message");
    }
  };
}

peer::behavior_type peer_fun(peer::broker_pointer self, connection_handle hdl,
                             const actor& buddy) {
  CAF_MESSAGE("peer_fun called");
  CAF_CHECK(self != nullptr);
  CAF_CHECK(buddy != invalid_actor);
  self->monitor(buddy);
  // assume exactly one connection
  auto cons = self->connections();
  if (cons.size() != 1) {
    cerr << "expected 1 connection, found " << cons.size() << endl;
    throw std::logic_error("num_connections() != 1");
  }
  self->configure_read(
    hdl, receive_policy::exactly(sizeof(atom_value) + sizeof(int)));
  auto write = [=](atom_value type, int value) {
    auto& buf = self->wr_buf(hdl);
    auto first = reinterpret_cast<char*>(&type);
    buf.insert(buf.end(), first, first + sizeof(atom_value));
    first = reinterpret_cast<char*>(&value);
    buf.insert(buf.end(), first, first + sizeof(int));
    self->flush(hdl);
  };
  return {
    [=](const connection_closed_msg&) {
      CAF_MESSAGE("received connection_closed_msg");
      self->quit();
    },
    [=](const new_data_msg& msg) {
      CAF_MESSAGE("received new_data_msg");
      atom_value type;
      int value;
      memcpy(&type, msg.buf.data(), sizeof(atom_value));
      memcpy(&value, msg.buf.data() + sizeof(atom_value), sizeof(int));
      self->send(buddy, type, value);
    },
    [=](ping_atom, int value) {
      CAF_MESSAGE("received ping{" << value << "}");
      write(ping_atom::value, value);
    },
    [=](pong_atom, int value) {
      CAF_MESSAGE("received pong{" << value << "}");
      write(pong_atom::value, value);
    },
    [=](const down_msg& dm) {
      CAF_MESSAGE("received down_msg");
      if (dm.source == buddy) {
        self->quit(dm.reason);
      }
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
    [=](publish_atom) -> maybe<uint16_t> {
      return get<1>(self->add_tcp_doorman(0, "127.0.0.1"));
    }
  };
}

void run_client(int argc, char** argv, uint16_t port) {
  actor_system system{actor_system_config{argc, argv}.load<io::middleman>()};
  auto p = system.spawn(ping, size_t{10});
  CAF_MESSAGE("spawn_client_typed...");
  auto cl = system.middleman().spawn_client(peer_fun, "localhost", port, p);
  CAF_REQUIRE(cl);
  CAF_MESSAGE("spawn_client_typed finished");
  anon_send(p, kickoff_atom::value, *cl);
  CAF_MESSAGE("`kickoff_atom` has been send");
}

void run_server(int argc, char** argv) {
  actor_system system{actor_system_config{argc, argv}.load<io::middleman>()};
  scoped_actor self{system};
  auto serv = system.middleman().spawn_broker(acceptor_fun, system.spawn(pong));
  std::thread child;
  self->request(serv, publish_atom::value).receive(
    [&](uint16_t port) {
      CAF_MESSAGE("server is running on port " << port);
      child = std::thread([=] {
        run_client(argc, argv, port);
      });
    }
  );
  self->await_all_other_actors_done();
  CAF_MESSAGE("wait for client system");
  child.join();
}

} // namespace <anonymous>

CAF_TEST(test_typed_broker) {
  auto argc = test::engine::argc();
  auto argv = test::engine::argv();
  run_server(argc, argv);
}
