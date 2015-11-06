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

#define CAF_SUITE io_dynamic_broker
#include "caf/test/unit_test.hpp"

#include <memory>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/string_algorithms.hpp"

#include "caf/detail/run_sub_unit_test.hpp"

#ifdef CAF_USE_ASIO
#include "caf/io/network/asio_multiplexer.hpp"
#endif // CAF_USE_ASIO

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
      },
      others >> [=] {
        CAF_TEST_ERROR("Unexpected message");
      });
    },
    others >> [=] {
      CAF_TEST_ERROR("Unexpected message");
    }
  );
}

void pong(event_based_actor* self) {
  CAF_MESSAGE("pong actor started");
  self->become(
    [=](ping_atom, int value) -> std::tuple<atom_value, int> {
      CAF_MESSAGE("received `ping_atom`");
      self->monitor(self->current_sender());
      // set next behavior
      self->become(
        [](ping_atom, int val) {
          return std::make_tuple(pong_atom::value, val);
        },
        [=](const down_msg& dm) {
          CAF_MESSAGE("received down_msg{" << dm.reason << "}");
          self->quit(dm.reason);
        },
        others >> [=] {
          CAF_TEST_ERROR("Unexpected message");
        }
      );
      // reply to 'ping'
      return std::make_tuple(pong_atom::value, value);
    },
    others >> [=] {
      CAF_TEST_ERROR("Unexpected message");
    }
  );
}

void peer_fun(broker* self, connection_handle hdl, const actor& buddy) {
  CAF_MESSAGE("peer_fun called");
  CAF_REQUIRE(self->subtype() == resumable::io_actor);
  CAF_CHECK(self != nullptr);
  CAF_CHECK(buddy != invalid_actor);
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
      CAF_MESSAGE("received: " << to_string(self->current_message()));
      write(ping_atom::value, value);
    },
    [=](pong_atom, int value) {
      CAF_MESSAGE("received: " << to_string(self->current_message()));
      write(pong_atom::value, value);
    },
    [=](const down_msg& dm) {
      CAF_MESSAGE("received: " << to_string(self->current_message()));
      if (dm.source == buddy)
        self->quit(dm.reason);
    },
    others >> [=] {
      CAF_MESSAGE("unexpected: " << to_string(self->current_message()));
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
    [=](publish_atom) -> uint16_t {
      return self->add_tcp_doorman(0, "127.0.0.1").second;
    },
    others >> [&] {
      CAF_TEST_ERROR("Unexpected message");
    }
  };
}

void run_server(actor_system& system, bool spawn_client,
                const char* bin_path, bool use_asio) {
  scoped_actor self{system};
  CAF_MESSAGE("spawn peer acceptor");
  auto serv = system.middleman().spawn_broker(peer_acceptor_fun,
                                              system.spawn(pong));
  self->sync_send(serv, publish_atom::value).await(
    [&](uint16_t port) {
      CAF_MESSAGE("server is running on port " << port);
      if (spawn_client) {
        auto child = detail::run_sub_unit_test(self,
                                               bin_path,
                                               test::engine::max_runtime(),
                                               CAF_XSTR(CAF_SUITE),
                                               use_asio,
                                               {"--client-port="
                                                + std::to_string(port)});
        child.join();
        self->receive(
          [](const std::string& output) {
            cout << endl << endl << "*** output of client program ***"
                 << endl << output << endl;
          }
        );
      }
    },
    others >> [&] {
      CAF_TEST_ERROR("unexpected message: "
                     << to_string(self->current_message()));
    }
  );
}

} // namespace <anonymous>

CAF_TEST(test_broker) {
  auto argc = test::engine::argc();
  auto argv = test::engine::argv();
  uint16_t port = 0;
  auto r = message_builder(argv, argv + argc).extract_opts({
    {"client-port,c", "set port for IO client", port},
    {"server,s", "run in server mode"},
    {"use-asio", "use ASIO network backend (if available)"}
  });
  if (! r.error.empty() || r.opts.count("help") > 0 || ! r.remainder.empty()) {
    cout << r.error << endl << endl << r.helptext << endl;
    return;
  }
  auto use_asio = r.opts.count("use-asio") > 0;
  actor_system_config cfg;
# ifdef CAF_USE_ASIO
  if (use_asio) {
    cfg.load<io::middleman, io::network::asio_multiplexer>());
  else
# endif // CAF_USE_ASIO
    cfg.load<io::middleman>();
  actor_system system{cfg};
  if (r.opts.count("client-port") > 0) {
    auto p = system.spawn(ping, size_t{10});
    CAF_MESSAGE("spawn_client...");
    auto cl = system.middleman().spawn_client(peer_fun, "localhost", port, p);
    CAF_REQUIRE(cl);
    CAF_MESSAGE("spawn_client finished");
    anon_send(p, kickoff_atom::value, *cl);
    CAF_MESSAGE("`kickoff_atom` has been send");
  } else if (r.opts.count("server") > 0) {
    // run in server mode
    run_server(system, false, "", use_asio);
  } else {
    run_server(system, true, test::engine::path(), use_asio);
  }
  system.await_all_actors_done();
}
