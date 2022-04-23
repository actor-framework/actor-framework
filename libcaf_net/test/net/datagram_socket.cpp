// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.datagram_socket

#include "caf/net/datagram_socket.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;
using namespace caf::net;

CAF_TEST(invalid_socket) {
  datagram_socket x;
  CAF_CHECK_NOT_EQUAL(allow_connreset(x, true), none);
}
