// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE io.network.default_multiplexer

#include "caf/io/network/default_multiplexer.hpp"

#include "io-test.hpp"

#include <algorithm>
#include <vector>

#include "caf/all.hpp"
#include "caf/io/all.hpp"
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

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(doorman io_failure) {
  MESSAGE("add doorman to server");
  // The multiplexer adds a pipe reader on startup.
  CHECK_EQ(server.mpx.num_socket_handlers(), 1u);
  auto doorman = unbox(server.mpx.new_tcp_doorman(0, nullptr, false));
  doorman->add_to_loop();
  server.mpx.handle_internal_events();
  CHECK_EQ(server.mpx.num_socket_handlers(), 2u);
  MESSAGE("trigger I/O failure in doorman");
  doorman->io_failure(&server.mpx, io::network::operation::propagate_error);
  server.mpx.handle_internal_events();
  CHECK_EQ(server.mpx.num_socket_handlers(), 1u);
}

END_FIXTURE_SCOPE()
