// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/socket.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using namespace caf::net;

TEST("invalid socket") {
  auto x = invalid_socket;
  check_eq(x.id, invalid_socket_id);
  check_eq(child_process_inherit(x, true), sec::network_syscall_failed);
  check_eq(nonblocking(x, true), sec::network_syscall_failed);
}
