// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.socket_guard

#include "caf/net/socket_guard.hpp"

#include "caf/test/dsl.hpp"

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

} // namespace

CAF_TEST_FIXTURE_SCOPE(socket_guard_tests, fixture)

CAF_TEST(cleanup) {
  {
    auto guard = make_socket_guard(sock);
    CAF_CHECK_EQUAL(guard.socket().id, dummy_id);
  }
  CAF_CHECK(sock.closed);
}

CAF_TEST(reset) {
  {
    auto guard = make_socket_guard(sock);
    CAF_CHECK_EQUAL(guard.socket().id, dummy_id);
    guard.release();
    CAF_CHECK_EQUAL(guard.socket().id, invalid_socket_id);
    guard.reset(sock);
    CAF_CHECK_EQUAL(guard.socket().id, dummy_id);
  }
  CAF_CHECK_EQUAL(sock.closed, true);
}

CAF_TEST(release) {
  {
    auto guard = make_socket_guard(sock);
    CAF_CHECK_EQUAL(guard.socket().id, dummy_id);
    guard.release();
    CAF_CHECK_EQUAL(guard.socket().id, invalid_socket_id);
  }
  CAF_CHECK_EQUAL(sock.closed, false);
}

CAF_TEST_FIXTURE_SCOPE_END()
