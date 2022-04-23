// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.tcp_accept_socket

#include "caf/net/tcp_accept_socket.hpp"

#include "caf/test/dsl.hpp"

#include "caf/net/socket_guard.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/uri.hpp"

using namespace caf;
using namespace caf::net;
using namespace std::literals::string_literals;

namespace {

struct fixture : test_coordinator_fixture<> {
  fixture() {
    auth.port = 0;
    auth.host = "0.0.0.0"s;
  }

  uri::authority_type auth;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(doorman_tests, fixture)

CAF_TEST(tcp connect) {
  auto acceptor = unbox(make_tcp_accept_socket(auth, false));
  auto port = unbox(local_port(socket_cast<network_socket>(acceptor)));
  auto acceptor_guard = make_socket_guard(acceptor);
  CAF_MESSAGE("opened acceptor on port " << port);
  uri::authority_type dst;
  dst.port = port;
  dst.host = "localhost"s;
  auto conn = make_socket_guard(unbox(make_connected_tcp_stream_socket(dst)));
  auto accepted = make_socket_guard(unbox(accept(acceptor)));
  CAF_MESSAGE("accepted connection");
}

CAF_TEST_FIXTURE_SCOPE_END()
