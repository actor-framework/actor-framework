// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/socket_guard.hpp"

#include "caf/event_based_actor.hpp"
#define CAF_SUITE net.datagram_transport

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/net/datagram_transport.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/udp_datagram_socket.hpp"

#include "caf/byte_buffer.hpp"
#include "caf/log/test.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace caf::net;
using namespace std::literals;

namespace {

behavior dummy_actor(event_based_actor*) {
  return {[](ip_endpoint, std::string) {}, [](const message&) {}};
}

constexpr std::string_view message = "hello manager!";

WITH_FIXTURE(test::fixture::deterministic) {

TEST("transfer data over datagram socket") {
  auto unbox = [this](auto x) {
    if (!x)
      fail("Error: {}", x.error());
    return std::move(*x);
  };
  auto mpx = net::multiplexer::make(nullptr);
  mpx->set_thread_id();
  if (auto err = mpx->init())
    fail("mpx->init() failed with: {}", err);
  auto addresses = ip::local_addresses("localhost");
  require(!addresses.empty());
  auto ep = ip_endpoint(addresses.front(), 0);
  auto recv_sock = unbox(make_udp_datagram_socket(ep));
  auto recv_sock_guard = make_socket_guard(recv_sock);
  auto recv_ep = ip_endpoint(ep.address(), unbox(local_port(recv_sock)));
  auto send_sock = unbox(make_udp_datagram_socket(ep));
  auto send_sock_guard = make_socket_guard(send_sock);
  auto send_ep = ip_endpoint(ep.address(), unbox(local_port(send_sock)));
  if (auto err = nonblocking(recv_sock, true))
    fail("nonblocking() returned an error: {}", err);
  if (auto err = nonblocking(send_sock, true))
    fail("nonblocking() returned an error: {}", err);
  auto dummy = sys.spawn(dummy_actor);
  auto transport = std::make_unique<caf::net::datagram_transport>(
    recv_sock, sys, mpx.get(), dummy);
  auto transport_ptr = transport.get();
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  if (auto err = mgr->start())
    fail("mgr->start() failed with: {}", err);
  mpx->apply_updates();
  check_eq(mpx->num_socket_managers(), 2u);
  SECTION("reading data from socket") {
    require_eq(write(send_sock, as_bytes(make_span(message)), recv_ep),
               static_cast<ptrdiff_t>(message.size()));
    log::test::debug("wrote {} bytes", message.size());
    mpx->poll_once(false);
    expect<ip_endpoint, std::string>().with(send_ep, message).to(dummy);
  }
  SECTION("writing data to socket") {
    anon_send(transport_ptr->actor_handle(), send_ep, std::string{message});
    std::this_thread::sleep_for(100ms);
    mpx->apply_updates();
    mpx->poll_once(false);
    byte_buffer recv_buffer;
    recv_buffer.resize(message.size());
    read(send_sock, recv_buffer, &ep);
    check_eq(to_string_view(recv_buffer), message);
    check_eq(ep, recv_ep);
  }
  anon_send_exit(transport_ptr->actor_handle(), exit_reason::user_shutdown);
  mpx->apply_updates();
  anon_send_exit(dummy, exit_reason::user_shutdown);
  while (mpx->poll_once(false)) {
    // repeat
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
