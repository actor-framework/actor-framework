// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/datagram_socket.hpp"

#include "caf/test/test.hpp"

using namespace caf;
using namespace caf::net;

namespace {

TEST("invalid_socket") {
  datagram_socket x;
  check_ne(allow_connreset(x, true), none);
}

} // namespace
