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

#define CAF_SUITE io_dynamic_broker
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

using ping_atom = caf::atom_constant<caf::atom("ping")>;
using pong_atom = caf::atom_constant<caf::atom("pong")>;
using publish_atom = caf::atom_constant<caf::atom("publish")>;
using kickoff_atom = caf::atom_constant<caf::atom("kickoff")>;

void ping(event_based_actor* self, size_t num_pings) {
  CAF_MESSAGE("num_pings: " << num_pings);
  auto count = std::make_shared<size_t>(0);
  self->become(
    [=](kickoff_atom, const actor& pong) {
      CAF_MESSAGE("received `kickoff_atom`");
      self->send(pong, ping_atom::value, 1);
      self->become(
        [=](pong_atom, int value)->std::tuple<atom_value, int> {
          if (++*count >= num_pings) {
            CAF_MESSAGE("received " << num_pings
                        << " pings, call self->quit");
            self->quit();
          }
          return std::make_tuple(ping_atom::value, value + 1);
        }
      );
    }
  );
}

void pong(event_based_actor* self) {
  CAF_MESSAGE("pong actor started");
  self->set_down_handler([=](down_msg& dm) {
    CAF_MESSAGE("received down_msg{" << to_string(dm.reason) << "}");
    self->quit(dm.reason);
  });
  self->become(
    [=](ping_atom, int value) -> std::tuple<atom_value, int> {
      CAF_MESSAGE("received `ping_atom`");
      self->monitor(self->current_sender());
      // set next behavior
      self->become(
        [](ping_atom, int val) {
          return std::make_tuple(pong_atom::value, val);
        }
      );
      // reply to 'ping'
      return std::make_tuple(pong_atom::value, value);
    }
  );
}

void peer_fun(broker* self, connection_handle hdl, const actor& buddy) {
  CAF_MESSAGE("peer_fun called");
  CAF_REQUIRE(self->subtype() == resumable::io_actor);
  CAF_CHECK(self != nullptr);
  self->monitor(buddy);
  // assume exactly one connection
  CAF_REQUIRE(self->connections().size() == 1);
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
  self->set_down_handler([=](down_msg& dm) {
    CAF_MESSAGE("received: " << to_string(dm));
    if (dm.source == buddy)
      self->quit(dm.reason);
  });
  self->become(
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
      CAF_MESSAGE("received: ping " << value);
      write(ping_atom::value, value);
    },
    [=](pong_atom, int value) {
      CAF_MESSAGE("received: pong " << value);
      write(pong_atom::value, value);
    }
  );
}

behavior peer_acceptor_fun(broker* self, const actor& buddy) {
  CAF_MESSAGE("peer_acceptor_fun");
  return {
    [=](const new_connection_msg& msg) {
      CAF_MESSAGE("received `new_connection_msg`");
      self->fork(peer_fun, msg.handle, buddy);
      self->quit();
    },
    [=](publish_atom) -> expected<uint16_t> {
      auto res = self->add_tcp_doorman(0, "127.0.0.1");
      if (!res)
        return std::move(res.error());
      return res->second;
    }
  };
}

void run_client(int argc, char** argv, uint16_t port) {
  actor_system_config cfg;
  actor_system system{cfg.load<io::middleman>().parse(argc, argv)};
  auto p = system.spawn(ping, size_t{10});
  CAF_MESSAGE("spawn_client...");
  auto cl = unbox(system.middleman().spawn_client(peer_fun,
                                                  "127.0.0.1", port, p));
  CAF_MESSAGE("spawn_client finished");
  anon_send(p, kickoff_atom::value, cl);
  CAF_MESSAGE("`kickoff_atom` has been send");
}

void run_server(int argc, char** argv) {
  actor_system_config cfg;
  actor_system system{cfg.load<io::middleman>().parse(argc, argv)};
  scoped_actor self{system};
  CAF_MESSAGE("spawn peer acceptor");
  auto serv = system.middleman().spawn_broker(peer_acceptor_fun,
                                              system.spawn(pong));
  std::thread child;
  self->request(serv, infinite, publish_atom::value).receive(
    [&](uint16_t port) {
      CAF_MESSAGE("server is running on port " << port);
      child = std::thread([=] {
        run_client(argc, argv, port);
      });
    },
    [&](const error& err) {
      CAF_ERROR("Error: " << self->system().render(err));
    }
  );
  self->await_all_other_actors_done();
  child.join();
}

} // namespace <anonymous>

CAF_TEST(test_broker) {
  auto argc = test::engine::argc();
  auto argv = test::engine::argv();
  run_server(argc, argv);
}
