// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/test.hpp"

#include "caf/net/socket_guard.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/log/test.hpp"
#include "caf/uri.hpp"

using namespace caf;
using namespace caf::net;
using namespace std::literals::string_literals;

namespace {

template <class T>
T unbox(caf::expected<T> x) {
  if (!x)
    test::runnable::current().fail("{}", to_string(x.error()));
  return std::move(*x);
}

struct fixture {
  fixture() {
    auth.port = 0;
    auth.host = "0.0.0.0"s;
  }

  uri::authority_type auth;
};

WITH_FIXTURE(fixture) {

TEST("tcp connect") {
  auto acceptor = unbox(make_tcp_accept_socket(auth, false));
  auto port = unbox(local_port(socket_cast<network_socket>(acceptor)));
  auto acceptor_guard = make_socket_guard(acceptor);
  log::test::debug("opened acceptor on port {}", port);
  uri::authority_type dst;
  dst.port = port;
  dst.host = "localhost"s;
  auto conn = make_socket_guard(unbox(make_connected_tcp_stream_socket(dst)));
  auto accepted = make_socket_guard(unbox(accept(acceptor)));
  log::test::debug("accepted connection");
}

} // WITH_FIXTURE(fixture)

} // namespace
