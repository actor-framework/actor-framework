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

#define CAF_SUITE io_newb_tcp

#include "caf/config.hpp"

#include "caf/test/dsl.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/io/newb.hpp"
#include "caf/policy/newb_raw.hpp"
#include "caf/policy/newb_tcp.hpp"

using namespace caf;
using namespace caf::io;
using namespace caf::policy;

namespace {

using alive_atom = atom_constant<atom("alive")>;
using start_atom = atom_constant<atom("start")>;
using shutdown_atom = atom_constant<atom("shutdown")>;

constexpr auto host = "127.0.0.1";

// behavior tcp_server(newb<new_raw_msg>* self, actor responder) {
//   self->send(responder, alive_atom::value);
//   self->configure_read(io::receive_policy::exactly(sizeof(uint32_t)));
//   return {
//     [=](new_raw_msg& msg) {
//       // Read message.
//       uint32_t data;
//       binary_deserializer bd(self->system(), msg.payload, msg.payload_len);
//       bd(data);
//       // Write message.
//       auto whdl = self->wr_buf(nullptr);
//       binary_serializer bs(&self->backend(), *whdl.buf);
//       bs(data + 1);
//     },
//     [=](io_error_msg& msg) {
//       CAF_FAIL("server got io error: " << to_string(msg.op));
//       self->quit();
//       self->stop();
//     },
//     [=](caf::exit_msg&) {
//       CAF_MESSAGE("parent shut down, doing the same");
//       self->stop();
//       self->quit();
//       self->send(responder, shutdown_atom::value);
//     }
//   };
// }

behavior tcp_server(newb<new_raw_msg>* self, actor responder) {
  self->configure_read(io::receive_policy::exactly(sizeof(uint32_t)));
  return {
    [=](new_raw_msg& msg) {
      // Read message.
      uint32_t data;
      binary_deserializer bd(self->system(), msg.payload, msg.payload_len);
      bd(data);
      CAF_MESSAGE("server got message from client: " << data);
      // Write message.
      auto whdl = self->wr_buf(nullptr);
      binary_serializer bs(&self->backend(), *whdl.buf);
      bs(data + 1);
    },
    [=](io_error_msg&) {
      CAF_MESSAGE("server: connection lost");
      self->quit();
      self->stop();
      self->send(responder, shutdown_atom::value);
    },
    [=](caf::exit_msg&) {
      CAF_MESSAGE("parent shut down, doing the same");
      self->stop();
      self->quit();
    }
  };
}

behavior tcp_client(newb<new_raw_msg>* self, uint32_t value) {
  self->configure_read(io::receive_policy::exactly(sizeof(uint32_t)));
  auto whdl = self->wr_buf(nullptr);
  binary_serializer bs(&self->backend(), *whdl.buf);
  bs(value);
  return {
    [=](new_raw_msg& msg) {
      CAF_MESSAGE("client received answer from server");
      uint32_t response;
      binary_deserializer bd(self->system(), msg.payload, msg.payload_len);
      bd(response);
      CAF_CHECK_EQUAL(response, value + 1);
      self->stop();
      self->quit();
    },
    [=](io_error_msg&) {
      CAF_MESSAGE("client: connection lost");
      // self->send(responder, quit_atom::value);
      self->stop();
      self->quit();
    }
  };
}

} // namespace anonymous

CAF_TEST(newb tcp communication) {
  actor_system_config config;
  config.load<io::middleman>();
  actor_system system{config};
  {
    scoped_actor self{system};
    // Create acceptor.
    accept_ptr<policy::new_raw_msg> pol{new accept_tcp<policy::new_raw_msg>};
    auto eserver = io::spawn_server<tcp_protocol<raw>>(system, tcp_server,
                                                       std::move(pol), 0,
                                                       nullptr, true, self);
    if (!eserver)
      CAF_FAIL("failed to start server: " << system.render(eserver.error()));
    uint16_t port;
    self->send(*eserver, port_atom::value);
    self->receive(
      [&](uint16_t published_on) {
        port = published_on;
        CAF_MESSAGE("server listening on port " << port);
      }
    );
    transport_ptr transport{new tcp_transport};
    auto eclient = io::spawn_client<tcp_protocol<raw>>(system, tcp_client,
                                                       std::move(transport),
                                                       host, port, 23);
    if (!eclient)
      CAF_FAIL("failed to start client: " << system.render(eclient.error()));
  }
  system.await_all_actors_done();
}
