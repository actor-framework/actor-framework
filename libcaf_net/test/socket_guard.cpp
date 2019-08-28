/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE socket_guard

#include "caf/net/socket_guard.hpp"

#include "caf/test/dsl.hpp"

#include "caf/net/abstract_socket.hpp"
#include "caf/net/socket_id.hpp"

using namespace caf;
using namespace caf::net;

namespace {

constexpr socket_id dummy_id = 13;

struct dummy_socket {
  dummy_socket(socket_id& id, bool& closed) : id(id), closed(closed) {
    // nop
  }

  dummy_socket& operator=(const dummy_socket& other) {
    id = other.id;
    closed = other.closed;
    return *this;
  }

  socket_id& id;
  bool& closed;
};

void close(dummy_socket x) {
  x.closed = true;
}

} // namespace

CAF_TEST(cleanup) {
  auto id = dummy_id;
  auto closed = false;
  dummy_socket sock{id, closed};
  {
    auto guard = make_socket_guard(sock);
    CAF_CHECK_EQUAL(sock.id, dummy_id);
  }
  CAF_CHECK_EQUAL(sock.id, invalid_socket_id);
  CAF_CHECK(sock.closed);
}

CAF_TEST(release) {
  auto id = dummy_id;
  auto closed = false;
  dummy_socket sock{id, closed};
  {
    auto guard = make_socket_guard(sock);
    CAF_CHECK_EQUAL(sock.id, dummy_id);
    auto released = guard.release();
    CAF_CHECK_EQUAL(sock.id, invalid_socket_id);
    sock = released;
  }
  // Cannot check socket.id because it is a reference and will be invalidated.
  CAF_CHECK_EQUAL(sock.closed, false);
}
