// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/tcp_stream_socket.hpp"

#include "caf/test/test.hpp"

#include "caf/net/ip.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include "caf/log/test.hpp"

using namespace caf;
using namespace caf::net;
using namespace std::literals;

namespace {

TEST("opening and connecting via tcp socket") {
  auto unbox = [this](auto expected) {
    if (expected)
      return *expected;
    else
      fail("expected<> contains error {}", expected.error());
  };
  uri::authority_type auth;
  auth.port = 0;
  auth.host = "0.0.0.0"s;
  auto acceptor = unbox(make_tcp_accept_socket(auth, false));
  auto acceptor_guard = make_socket_guard(acceptor);
  auto port = unbox(local_port(acceptor));
  check_ne(port, 0u);
  log::test::debug("opened ipv4 acceptor on port {}", port);
  SECTION("connecting using auth") {
    uri::authority_type dst;
    dst.host = "localhost"s;
    dst.port = port;
    auto conn = unbox(make_connected_tcp_stream_socket(dst));
    auto conn_guard = make_socket_guard(conn);
    check_ne(conn, invalid_socket);
    auto accepted = unbox(accept(acceptor));
    auto accepted_guard = make_socket_guard(conn);
    check_ne(accepted, invalid_socket);
    log::test::debug("connected");
  }
  SECTION("connecting using host and port") {
    auto conn = unbox(make_connected_tcp_stream_socket("localhost"s, port));
    auto conn_guard = make_socket_guard(conn);
    check_ne(conn, invalid_socket);
    auto accepted = unbox(accept(acceptor));
    auto accepted_guard = make_socket_guard(conn);
    check_ne(accepted, invalid_socket);
    log::test::debug("connected");
  }
  SECTION("using local ip addr") {
    uri::authority_type dst;
    dst.host = ip_address{make_ipv4_address(127, 0, 0, 1)};
    dst.port = port;
    auto conn = unbox(make_connected_tcp_stream_socket(dst));
    auto conn_guard = make_socket_guard(conn);
    check_ne(conn, invalid_socket);
    auto accepted = unbox(accept(acceptor));
    auto accepted_guard = make_socket_guard(conn);
    check_ne(accepted, invalid_socket);
    log::test::debug("connected");
  }
}

TEST("opening and connecting to ipv6 using tcp socket") {
  auto unbox = [this](auto expected) {
    if (expected)
      return *expected;
    else
      fail("expected<> contains error {}", expected.error());
  };
  auto addrs = ip::local_addresses("localhost");
  if (auto it = std::find_if(addrs.begin(), addrs.end(),
                             [](auto& ip) { return !ip.embeds_v4(); });
      it != addrs.end()) {
    uri::authority_type auth;
    auth.port = 0;
    auth.host = *it;
    auto acceptor = unbox(make_tcp_accept_socket(auth, false));
    auto acceptor_guard = make_socket_guard(acceptor);
    auto port = unbox(local_port(acceptor));
    check_ne(port, 0u);
    log::test::debug("opened ipv6 acceptor on port {}", port);
    SECTION("connecting using auth") {
      uri::authority_type dst;
      dst.host = "::1"s;
      dst.port = port;
      auto conn = unbox(make_connected_tcp_stream_socket(dst));
      auto conn_guard = make_socket_guard(conn);
      check_ne(conn, invalid_socket);
      auto accepted = unbox(accept(acceptor));
      auto accepted_guard = make_socket_guard(conn);
      check_ne(accepted, invalid_socket);
      log::test::debug("connected");
    }
    SECTION("connecting using host and port") {
      auto conn = unbox(make_connected_tcp_stream_socket("::1"s, port));
      auto conn_guard = make_socket_guard(conn);
      check_ne(conn, invalid_socket);
      auto accepted = unbox(accept(acceptor));
      auto accepted_guard = make_socket_guard(conn);
      check_ne(accepted, invalid_socket);
      log::test::debug("connected");
    }
    SECTION("using local ip addr") {
      uri::authority_type dst;
      dst.host = ip_address{{0}, {0x1}};
      dst.port = port;
      auto conn = unbox(make_connected_tcp_stream_socket(dst));
      auto conn_guard = make_socket_guard(conn);
      check_ne(conn, invalid_socket);
      auto accepted = unbox(accept(acceptor));
      auto accepted_guard = make_socket_guard(conn);
      check_ne(accepted, invalid_socket);
      log::test::debug("connected");
    }
  }
}

TEST("tcp connect to invalid destination") {
  SECTION("invalid port") {
    auto conn = make_connected_tcp_stream_socket("localhost"s, 0);
    check(!conn.has_value());
  }
  SECTION("invalid domain") {
    auto conn = make_connected_tcp_stream_socket("example.test", 80);
    check(!conn.has_value());
  }
  SECTION("connect with retry to invalid port") {
    auto conn = detail::tcp_try_connect("localhost"s, 0, 5s, 5, 10ms);
    check(!conn.has_value());
  }
  SECTION("connect with retry to invalid domain") {
    auto conn = detail::tcp_try_connect("example.test"s, 80, 5s, 5, 10ms);
    check(!conn.has_value());
  }
}

} // namespace
