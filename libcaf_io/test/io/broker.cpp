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

#define CAF_SUITE io.broker

#include "caf/io/broker.hpp"

#include "caf/test/io_dsl.hpp"

#include <iostream>
#include <memory>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;
using namespace caf::io;

namespace {

using ping_atom = caf::atom_constant<caf::atom("ping")>;
using pong_atom = caf::atom_constant<caf::atom("pong")>;
using publish_atom = caf::atom_constant<caf::atom("publish")>;
using kickoff_atom = caf::atom_constant<caf::atom("kickoff")>;

struct suite_state {
  int pings = 0;
  int pongs = 0;
  suite_state() = default;
};

using suite_state_ptr = std::shared_ptr<suite_state>;

behavior ping(event_based_actor* self, suite_state_ptr ssp) {
  return {
    [=](kickoff_atom, const actor& pong) {
      CAF_MESSAGE("received `kickoff_atom`");
      ++ssp->pings;
      self->send(pong, ping_atom::value);
      self->become([=](pong_atom) {
        CAF_MESSAGE("ping: received pong");
        self->send(pong, ping_atom::value);
        if (++ssp->pings == 10) {
          self->quit();
          CAF_MESSAGE("ping is done");
        }
      });
    },
  };
}

behavior pong(event_based_actor* self, suite_state_ptr ssp) {
  return {
    [=](ping_atom) -> atom_value {
      CAF_MESSAGE("pong: received ping");
      if (++ssp->pongs == 10) {
        self->quit();
        CAF_MESSAGE("pong is done");
      }
      return pong_atom::value;
    },
  };
}

behavior peer_fun(broker* self, connection_handle hdl, const actor& buddy) {
  CAF_MESSAGE("peer_fun called");
  CAF_REQUIRE(self->subtype() == resumable::io_actor);
  CAF_CHECK(self != nullptr);
  self->monitor(buddy);
  self->set_down_handler([self](down_msg& dm) {
    // Stop if buddy is done.
    self->quit(std::move(dm.reason));
  });
  // Assume exactly one connection.
  CAF_REQUIRE(self->connections().size() == 1);
  self->configure_read(hdl, receive_policy::exactly(sizeof(atom_value)));
  auto write = [=](atom_value type) {
    auto& buf = self->wr_buf(hdl);
    auto first = reinterpret_cast<char*>(&type);
    buf.insert(buf.end(), first, first + sizeof(atom_value));
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
      memcpy(&type, msg.buf.data(), sizeof(atom_value));
      self->send(buddy, type);
    },
    [=](ping_atom) { write(ping_atom::value); },
    [=](pong_atom) { write(pong_atom::value); },
  };
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
      auto res = self->add_tcp_doorman(8080);
      if (!res)
        return std::move(res.error());
      return res->second;
    },
  };
}

using int_peer = connection_handler::extend<replies_to<int>::with<int>>;

int_peer::behavior_type int_peer_fun(int_peer::broker_pointer) {
  return {
    [=](const connection_closed_msg&) {
      CAF_FAIL("received connection_closed_msg");
    },
    [=](const new_data_msg&) { CAF_FAIL("received new_data_msg"); },
    [=](int value) {
      CAF_MESSAGE("received: " << value);
      return value;
    },
  };
}

} // namespace

CAF_TEST_FIXTURE_SCOPE(broker_tests, point_to_point_fixture<>)

CAF_TEST(test broker to broker communication) {
  prepare_connection(mars, earth, "mars", 8080);
  CAF_MESSAGE("spawn peer acceptor on mars");
  auto ssp = std::make_shared<suite_state>();
  auto server = mars.mm.spawn_broker(peer_acceptor_fun,
                                     mars.sys.spawn(pong, ssp));
  mars.self->send(server, publish_atom::value);
  run();
  expect_on(mars, (uint16_t), from(server).to(mars.self).with(8080));
  CAF_MESSAGE("spawn ping and client on earth");
  auto pinger = earth.sys.spawn(ping, ssp);
  auto client = unbox(earth.mm.spawn_client(peer_fun, "mars", 8080, pinger));
  anon_send(pinger, kickoff_atom::value, client);
  run();
  CAF_CHECK_EQUAL(ssp->pings, 10);
  CAF_CHECK_EQUAL(ssp->pongs, 10);
}

CAF_TEST(test whether we can spawn typed broker) {
  auto peer = mars.mm.spawn_broker(int_peer_fun);
  mars.self->send(peer, 42);
  run();
  expect_on(mars, (int), from(peer).to(mars.self).with(42));
}

CAF_TEST_FIXTURE_SCOPE_END()
