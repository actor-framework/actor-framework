// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.tcp_socket

#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/test/dsl.hpp"

#include "caf/net/socket_guard.hpp"

using namespace caf;
using namespace caf::net;

namespace {

// TODO: switch to std::operator""s when switching to C++14
std::string operator"" _s(const char* str, size_t size) {
  return std::string(str, size);
}

struct fixture {
  fixture() {
    auth.port = 0;
    auth.host = "0.0.0.0"_s;
  }

  uri::authority_type auth;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(tcp_sockets_tests, fixture)

CAF_TEST(open tcp port) {
  auto acceptor = unbox(make_tcp_accept_socket(auth, false));
  auto port = unbox(local_port(acceptor));
  CAF_CHECK_NOT_EQUAL(port, 0);
  auto acceptor_guard = make_socket_guard(acceptor);
  CAF_MESSAGE("opened acceptor on port " << port);
}

CAF_TEST(tcp connect) {
  auto acceptor = unbox(make_tcp_accept_socket(auth, false));
  auto port = unbox(local_port(acceptor));
  CAF_CHECK_NOT_EQUAL(port, 0);
  auto acceptor_guard = make_socket_guard(acceptor);
  CAF_MESSAGE("opened acceptor on port " << port);
  uri::authority_type dst;
  dst.port = port;
  dst.host = "localhost"_s;
  CAF_MESSAGE("connecting to localhost");
  auto conn = unbox(make_connected_tcp_stream_socket(dst));
  auto conn_guard = make_socket_guard(conn);
  CAF_CHECK_NOT_EQUAL(conn, invalid_socket);
  auto accepted = unbox(accept(acceptor));
  auto accepted_guard = make_socket_guard(conn);
  CAF_CHECK_NOT_EQUAL(accepted, invalid_socket);
  CAF_MESSAGE("connected");
}

CAF_TEST_FIXTURE_SCOPE_END()
