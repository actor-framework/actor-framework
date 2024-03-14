// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/network_socket.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using namespace caf::net;

namespace {

TEST("invalid socket") {
  network_socket x;
  check_eq(allow_udp_connreset(x, true), sec::network_syscall_failed);
  check_eq(send_buffer_size(x), sec::network_syscall_failed);
  check_eq(local_port(x), sec::network_syscall_failed);
  check_eq(local_addr(x), sec::network_syscall_failed);
  check_eq(remote_port(x), sec::network_syscall_failed);
  check_eq(remote_addr(x), sec::network_syscall_failed);
}

} // namespace
