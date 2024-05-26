// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/socket_guard.hpp"

#include "caf/test/test.hpp"

#include "caf/net/socket_id.hpp"

using namespace caf;
using namespace caf::net;

namespace {

constexpr socket_id dummy_id = 13;

struct dummy_socket {
  dummy_socket(socket_id id, bool& closed) : id(id), closed(closed) {
    // nop
  }

  dummy_socket(const dummy_socket&) = default;

  dummy_socket& operator=(const dummy_socket& other) {
    id = other.id;
    closed = other.closed;
    return *this;
  }

  socket_id id;
  bool& closed;
};

void close(dummy_socket x) {
  x.id = invalid_socket_id;
  x.closed = true;
}

struct fixture {
  fixture() : closed{false}, sock{dummy_id, closed} {
    // nop
  }

  bool closed;
  dummy_socket sock;
};

WITH_FIXTURE(fixture) {

TEST("cleanup") {
  {
    auto guard = make_socket_guard(sock);
    check_eq(guard.socket().id, dummy_id);
  }
  check(sock.closed);
}

TEST("reset") {
  {
    auto guard = make_socket_guard(sock);
    check_eq(guard.socket().id, dummy_id);
    guard.release();
    check_eq(guard.socket().id, invalid_socket_id);
    guard.reset(sock);
    check_eq(guard.socket().id, dummy_id);
  }
  check_eq(sock.closed, true);
}

TEST("release") {
  {
    auto guard = make_socket_guard(sock);
    check_eq(guard.socket().id, dummy_id);
    guard.release();
    check_eq(guard.socket().id, invalid_socket_id);
  }
  check_eq(sock.closed, false);
}

} // WITH_FIXTURE(fixture)

} // namespace
