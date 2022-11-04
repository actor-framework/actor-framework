// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE network_socket

#include "caf/net/network_socket.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

using namespace caf;
using namespace caf::net;

CAF_TEST_FIXTURE_SCOPE(network_socket_tests, host_fixture)

CAF_TEST(invalid socket) {
  network_socket x;
  CAF_CHECK_EQUAL(allow_udp_connreset(x, true), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(send_buffer_size(x), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(local_port(x), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(local_addr(x), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(remote_port(x), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(remote_addr(x), sec::network_syscall_failed);
}

CAF_TEST_FIXTURE_SCOPE_END()
