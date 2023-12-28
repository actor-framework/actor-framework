// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/tcp_accept_socket.hpp"

#include "caf/test/test.hpp"

#include "caf/net/ip.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/log/test.hpp"

using namespace caf;
using namespace caf::net;
using namespace std::literals;

namespace {

TEST("opening and accepting tcp on a socket") {
  uri::authority_type auth;
  auth.port = 0;
  SECTION("using the ipv4 loopback") {
    auto addrs = ip::resolve("0.0.0.0");
    check_eq(addrs.size(), 1u);
    addrs = ip::resolve("localhost");
    check_eq(addrs.size(), 2u);
    auth.host = "0.0.0.0"s;
    auto acceptor = make_tcp_accept_socket(auth, false);
    require(acceptor.has_value());
    auto acceptor_guard = make_socket_guard(*acceptor);
    auto port = local_port(*acceptor);
    check(port.has_value());
    check_ne(port, 0u);
    log::test::debug("opened ipv4 acceptor on port {}", *port);
    SECTION("fail opening another socket on the same port") {
      auth.port = *port;
      auto acceptor = make_tcp_accept_socket(auth, false);
      check(!acceptor.has_value());
    }
  }
  SECTION("try using the ipv6 loopback") {
    // make_tcp_accept_socket will fall back to ipv4 if it fails connecting
    // to ipv6, so we have to manually resolve the addresses first.
    // NOTE: some of our docker builders dont have ipv6 loopback.
    auto addrs = ip::local_addresses("localhost");
    if (auto it = std::find_if(addrs.begin(), addrs.end(),
                               [](auto& ip) { return !ip.embeds_v4(); });
        it != addrs.end()) {
      log::test::info("opening socket on {}:{}", *it, auth.port);
      auth.host = *it;
      auto acceptor = make_tcp_accept_socket(auth, false);
      if (!check(acceptor.has_value())) {
        check_eq(acceptor.error(), error{});
      }
      auto acceptor_guard = make_socket_guard(*acceptor);
      auto port = local_port(*acceptor);
      check(port.has_value());
      check_ne(port, 0u);
      log::test::debug("opened tcp acceptor on port {}", *port);
      SECTION("fail opening another socket on the same port") {
        auth.port = *port;
        auto second_acceptor = make_tcp_accept_socket(auth, false);
        check(!second_acceptor.has_value());
      }
    }
  }
}

} // namespace
