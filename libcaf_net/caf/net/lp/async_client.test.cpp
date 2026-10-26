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
#include "caf/detail/connector.hpp"
#include "caf/error.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/flow/observable_builder.hpp"

#include <memory>
#include <semaphore>

using namespace caf;
using namespace std::literals;

using caf::async::make_spsc_buffer_resource;

namespace {

namespace lp = caf::net::lp;

class blocking_connector_state {
public:
  std::binary_semaphore connect_started{0};
  std::binary_semaphore release_connect{0};
};

class blocking_connector : public detail::connector {
public:
  explicit blocking_connector(std::shared_ptr<blocking_connector_state> state)
    : state_(std::move(state)) {
    // nop
  }

  expected<net::stream_socket> connect(const std::string&, uint16_t, timespan,
                                       size_t, timespan) override {
    state_->connect_started.release();
    state_->release_connect.acquire();
    return caf::unexpected{make_error(sec::runtime_error)};
  }

private:
  std::shared_ptr<blocking_connector_state> state_;
};

} // namespace

TEST("LP acceptor memory leak regression") {
  caf::actor_system_config cfg;
  cfg.load<caf::net::middleman>();
  caf::actor_system sys{cfg};
  auto acceptor = unbox(net::make_tcp_accept_socket(0));
  auto maybe_hdl = lp::with(sys) //
                     .accept(acceptor)
                     .start([&sys](auto events) {
                       sys.spawn([events](event_based_actor* self) {
                         self->make_observable().from_resource(events).for_each(
                           [self](const auto& ev) {
                             auto [pull, push] = ev.data();
                             pull.observe_on(self).subscribe(push);
                           });
                       });
                     });
  auto server_hdl = unbox(maybe_hdl);
  // Do not run the multiplexer after disposal. The acceptor must clean up its
  // references while the actor system is shutting down. Otherwise, this test
  // triggers a memory leak (under ASan/LeakSan).
  server_hdl.dispose();
}

TEST("with_v2 connects asynchronously without blocking the caller") {
  caf::actor_system_config cfg;
  cfg.load<caf::net::middleman>();
  caf::actor_system sys{cfg};
  auto endpoint = caf::uri{};
  auto parse_err = caf::parse("tcp://127.0.0.1:1"s, endpoint);
  require(parse_err.empty());
  auto state = std::make_shared<blocking_connector_state>();
  auto buffers = lp::with_v2(sys)
                   .async()
                   .connect(std::move(endpoint))
                   .connector(std::make_unique<blocking_connector>(state))
                   .start();
  auto connect_called = state->connect_started.try_acquire_for(1s);
  state->release_connect.release();
  require(connect_called);
  auto in = caf::async::make_blocking_consumer(std::move(buffers.first));
  require(in.has_value());
  lp::frame received;
  auto res = in->pull(async::prioritize_errors, received, 2s);
  check_eq(res, async::read_result::abort);
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

TEST("with_v2 requires an explicit port in URIs") {
  caf::actor_system_config cfg;
  cfg.load<caf::net::middleman>();
  caf::actor_system sys{cfg};
  auto endpoint = caf::uri{};
  auto parse_err = caf::parse("tcp://127.0.0.1"s, endpoint);
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
  check_eq(in->abort_reason(), sec::invalid_argument);
}
