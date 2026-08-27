// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/test.hpp"

#include "caf/net/lp/frame.hpp"
#include "caf/net/lp/with.hpp"
#include "caf/net/lp/with_v2.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/tcp_accept_socket.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/async/blocking_consumer.hpp"
#include "caf/async/blocking_producer.hpp"
#include "caf/detail/format.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/flow/observable_builder.hpp"

using namespace caf;
using namespace std::literals;

using caf::async::make_spsc_buffer_resource;

namespace {

namespace lp = caf::net::lp;

/// Starts an echo server on the given accept socket.
expected<disposable> start_echo_server(actor_system& sys,
                                       net::tcp_accept_socket fd) {
  return lp::with(sys).accept(fd).start([&sys](auto events) {
    sys.spawn([events](event_based_actor* self) {
      self->make_observable().from_resource(events).for_each(
        [self](const auto& ev) {
          auto [pull, push] = ev.data();
          pull.observe_on(self).subscribe(push);
        });
    });
  });
}

} // namespace

TEST("with_v2 connects asynchronously without blocking the caller") {
  caf::actor_system_config cfg;
  cfg.load<caf::net::middleman>();
  caf::actor_system sys{cfg};
  auto acceptor = unbox(net::make_tcp_accept_socket(0));
  auto port = unbox(net::local_port(acceptor));
  auto server_hdl = unbox(start_echo_server(sys, acceptor));
  detail::scope_guard server_guard{
    [server_hdl]() mutable noexcept { server_hdl.dispose(); }};
  auto endpoint = caf::uri{};
  auto parse_err = caf::parse(caf::detail::format("tcp://127.0.0.1:{}", port),
                              endpoint);
  require(parse_err.empty());
  auto [pull, push] = make_spsc_buffer_resource<lp::frame>();
  auto [a2s_pull, a2s_push] = make_spsc_buffer_resource<lp::frame>();
  lp::with_v2(sys)
    .async()
    .connect(std::move(endpoint))
    .connection_timeout(1s)
    .max_retry_count(1)
    .start(std::move(a2s_pull), std::move(push));
  auto out = caf::async::make_blocking_producer(std::move(a2s_push));
  require(out.has_value());
  auto in = caf::async::make_blocking_consumer(std::move(pull));
  require(in.has_value());
  check(out->push(lp::frame{as_bytes(std::span{"hello"sv})}));
  lp::frame received;
  auto res = in->pull(async::prioritize_errors, received, 2s);
  require_eq(res, async::read_result::ok);
  auto received_str = std::string{};
  for (auto val : received.bytes())
    received_str.push_back(static_cast<char>(val));
  check_eq(received_str, "hello");
}

TEST("with_v2 reports errors by closing the buffer with an error") {
  caf::actor_system_config cfg;
  cfg.load<caf::net::middleman>();
  caf::actor_system sys{cfg};
  auto endpoint = caf::uri{};
  auto parse_err = caf::parse("ftp://example.org/"s, endpoint);
  require(parse_err.empty());
  auto [s2a_pull, s2a_push] = make_spsc_buffer_resource<lp::frame>();
  auto [a2s_pull, a2s_push] = make_spsc_buffer_resource<lp::frame>();
  lp::with_v2(sys)
    .async()
    .connect(std::move(endpoint))
    .start(std::move(a2s_pull), std::move(s2a_push));
  auto in = caf::async::make_blocking_consumer(std::move(s2a_pull));
  require(in.has_value());
  lp::frame received;
  auto res = in->pull(async::prioritize_errors, received, 2s);
  check_eq(res, async::read_result::abort);
}
