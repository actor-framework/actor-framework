// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE socket

#include "caf/net/socket.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

using namespace caf;
using namespace caf::net;

CAF_TEST_FIXTURE_SCOPE(socket_tests, host_fixture)

CAF_TEST(invalid socket) {
  auto x = invalid_socket;
  CAF_CHECK_EQUAL(x.id, invalid_socket_id);
  CAF_CHECK_EQUAL(child_process_inherit(x, true), sec::network_syscall_failed);
  CAF_CHECK_EQUAL(nonblocking(x, true), sec::network_syscall_failed);
}

CAF_TEST_FIXTURE_SCOPE_END()
