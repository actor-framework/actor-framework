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

#define CAF_SUITE net.backend.tcp

#include "caf/net/backend/tcp.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

#include "caf/ip_endpoint.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket_guard.hpp"

using namespace caf;
using namespace caf::net;

namespace {

struct config : actor_system_config {
  config() {
    put(content, "middleman.this-node", unbox(make_uri("tcp://earth")));
    load<middleman, backend::tcp>();
  }
};

struct fixture : test_coordinator_fixture<config>, host_fixture {
  using byte_buffer_ptr = std::shared_ptr<byte_buffer>;

  fixture() : mm{sys.network_manager()}, mpx{mm.mpx()} {
    mpx->set_thread_id();
    handle_io_events();
  }

  bool handle_io_event() override {
    return mpx->poll_once(false);
  }

  net::middleman& mm;
  const multiplexer_ptr& mpx;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(tcp_backend_tests, fixture)

CAF_TEST(doorman accept) {
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 2);
  auto backend = mm.backend("tcp");
  CAF_CHECK(backend);
  auto port = backend->port();
  CAF_MESSAGE("trying to connect to system with " << CAF_ARG(port));
  ip_endpoint ep;
  auto ep_str = std::string("[::1]:") + std::to_string(port);
  if (auto err = detail::parse(ep_str, ep))
    CAF_FAIL("could not parse " << CAF_ARG(ep_str) << " " << CAF_ARG(err));
  auto sock = make_connected_tcp_stream_socket(ep);
  if (!sock)
    CAF_FAIL("could not connect");
  auto guard = make_socket_guard(*sock);
  handle_io_event();
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 3);
}

CAF_TEST(connect) {
  ip_endpoint ep;
  if (auto err = detail::parse("[::]:0", ep))
    CAF_FAIL("could not parse endpoint" << err);
  auto acceptor = make_tcp_accept_socket(ep);
  CAF_CHECK(!acceptor)
  auto acc_guard = make_socket_guard(*acceptor);
  auto port = local_port(*acc_guard);
  auto uri_str = std::string("tcp://localhost:") + std::to_string(port);
  CAF_MESSAGE("connecting to " << CAF_ARG(uri_str));
  auto ptr = mm.backend("tcp");
  CAF_CHECK(mm->connect(make_uri(ep_string)));
}

CAF_TEST_FIXTURE_SCOPE_END()
