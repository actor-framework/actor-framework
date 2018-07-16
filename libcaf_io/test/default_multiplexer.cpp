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

#define CAF_SUITE io_default_multiplexer
#include "caf/test/io_dsl.hpp"

#include <vector>
#include <algorithm>

#include "caf/all.hpp"
#include "caf/io/all.hpp"
#include "caf/io/network/default_multiplexer.hpp"
#include "caf/io/network/operation.hpp"

using namespace caf;

namespace {

struct sub_fixture : test_coordinator_fixture<> {
  io::network::default_multiplexer mpx;

  sub_fixture() : mpx(&sys) {
    // nop
  }

  bool exec_all() {
    size_t count = 0;
    while (mpx.poll_once(false)) {
      ++count;
    }
    return count != 0;
  }
};

struct fixture {
  sub_fixture client;

  sub_fixture server;

  void exec_all() {
    while (client.exec_all() || server.exec_all()) {
      // Rince and repeat.
    }
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(default_multiplexer_tests, fixture)

CAF_TEST(doorman io_failure) {
  CAF_MESSAGE("add doorman to server");
  // The multiplexer adds a pipe reader on startup.
  CAF_CHECK_EQUAL(server.mpx.num_socket_handlers(), 1u);
  auto doorman = unbox(server.mpx.new_tcp_doorman(0, nullptr, false));
  doorman->add_to_loop();
  server.mpx.handle_internal_events();
  CAF_CHECK_EQUAL(server.mpx.num_socket_handlers(), 2u);
  CAF_MESSAGE("trigger I/O failure in doorman");
  doorman->io_failure(&server.mpx, io::network::operation::propagate_error);
  server.mpx.handle_internal_events();
  CAF_CHECK_EQUAL(server.mpx.num_socket_handlers(), 1u);
}

CAF_TEST(scribe io_failure) {
  CAF_MESSAGE("add doorman to server");
  CAF_CHECK_EQUAL(server.mpx.num_socket_handlers(), 1u);
  auto doorman = unbox(server.mpx.new_tcp_doorman(0, nullptr, false));
  doorman->add_to_loop();
  server.mpx.handle_internal_events();
  CAF_CHECK_EQUAL(server.mpx.num_socket_handlers(), 2u);
  CAF_MESSAGE("connect to server (add scribe to client)");
  auto scribe = unbox(client.mpx.new_tcp_scribe("localhost", doorman->port()));
  CAF_CHECK_EQUAL(client.mpx.num_socket_handlers(), 1u);
  scribe->add_to_loop();
  client.mpx.handle_internal_events();
  CAF_CHECK_EQUAL(client.mpx.num_socket_handlers(), 2u);
  CAF_MESSAGE("trigger I/O failure in scribe");
  scribe->io_failure(&client.mpx, io::network::operation::propagate_error);
  client.mpx.handle_internal_events();
  CAF_CHECK_EQUAL(client.mpx.num_socket_handlers(), 1u);
  CAF_MESSAGE("trigger I/O failure in doorman");
  doorman->io_failure(&server.mpx, io::network::operation::propagate_error);
  server.mpx.handle_internal_events();
  CAF_CHECK_EQUAL(server.mpx.num_socket_handlers(), 1u);
}

CAF_TEST_FIXTURE_SCOPE_END()
