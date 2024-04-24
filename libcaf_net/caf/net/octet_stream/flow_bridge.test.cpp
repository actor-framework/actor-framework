// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/octet_stream/flow_bridge.hpp"

#include "caf/test/test.hpp"

#include "caf/net/fwd.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/detail/latch.hpp"
#include "caf/flow/observable_builder.hpp"
#include "caf/flow/scoped_coordinator.hpp"
#include "caf/log/test.hpp"

using namespace caf;

using namespace std::literals;

namespace {

// Simple loop receiving `num_items` bytes and sending them back incremented by
// one.
void ping_pong(net::stream_socket fd, int num_items) {
  auto guard = detail::scope_guard{[fd]() noexcept { net::close(fd); }};
  for (int i = 0; i < num_items; ++i) {
    std::byte buf;
    if (net::read(fd, make_span(&buf, 1)) != 1) {
      return;
    }
    buf = std::byte{static_cast<uint8_t>(std::to_integer<int>(buf) + 1)};
    if (net::write(fd, make_span(&buf, 1)) != 1) {
      return;
    }
  }
}

struct fixture {
  fixture() {
    mpx = net::multiplexer::make(nullptr);
    std::ignore = mpx->init();
    mpx_thread = std::thread{[mpx = mpx] {
      mpx->set_thread_id();
      mpx->run();
    }};
  }

  ~fixture() {
    mpx->shutdown();
    mpx_thread.join();
  }

  net::multiplexer_ptr mpx;
  std::thread mpx_thread;
};

WITH_FIXTURE(fixture) {

TEST("a bridge makes received data available through an SPSC buffer") {
  struct trait {
    using input_type = int;
    using output_type = int;
    flow::observable<int> map_inputs(flow::coordinator*,
                                     flow::observable<std::byte> bytes) {
      return bytes //
        .map([](std::byte item) { return std::to_integer<int>(item); })
        .as_observable();
    }
    flow::observable<std::byte> map_outputs(flow::coordinator*,
                                            flow::observable<int> bytes) {
      return bytes //
        .map([](int item) { return static_cast<std::byte>(item); })
        .as_observable();
    }
  };
  // Socket and thread setup.
  auto fd_pair = net::make_stream_socket_pair();
  require(fd_pair.has_value());
  auto [fd1, fd2] = *fd_pair;
  auto ping_pong_thread = std::thread{ping_pong, fd2, 50};
  auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<int>();
  auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<int>();
  // Bridge setup.
  auto bridge = net::octet_stream::make_flow_bridge(16, 16, trait{},
                                                    std::move(s2a_pull),
                                                    std::move(a2s_push));
  auto transport = net::octet_stream::transport::make(fd1, std::move(bridge));
  transport->active_policy().connect();
  auto ptr = net::socket_manager::make(mpx.get(), std::move(transport));
  mpx->start(ptr);
  // Run the flow to completion.
  auto received = std::make_shared<std::vector<int>>();
  auto self = flow::scoped_coordinator::make();
  self->make_observable().iota(1).take(50).subscribe(std::move(s2a_push));
  a2s_pull.observe_on(self.get()).for_each([received](int item) {
    received->push_back(item);
  });
  self->run_some(std::chrono::seconds(1)); // Run at most for 1 second.
  // Check the results.
  auto want = std::vector<int>();
  want.resize(50);
  std::iota(want.begin(), want.end(), 2);
  check_eq(*received, want);
  // Cleanup.
  ping_pong_thread.join();
}

TEST("a bridge may use time-based flow operators") {
  struct trait {
    using input_type = int;
    using output_type = int;
    flow::observable<int> map_inputs(flow::coordinator*,
                                     flow::observable<std::byte> bytes) {
      return bytes //
        .map([](std::byte item) { return std::to_integer<int>(item); })
        .as_observable();
    }
    flow::observable<std::byte> map_outputs(flow::coordinator* self,
                                            flow::observable<int> bytes) {
      return bytes //
        .map([](int item) { return static_cast<std::byte>(item); })
        .merge(self
                 ->make_observable() //
                 .interval(1ms)
                 .take(5)
                 .map([val = 1](int64_t) mutable {
                   return static_cast<std::byte>(val++);
                 }))
        .as_observable();
    }
  };
  // Socket and thread setup.
  auto fd_pair = net::make_stream_socket_pair();
  require(fd_pair.has_value());
  auto [fd1, fd2] = *fd_pair;
  auto ping_pong_thread = std::thread{ping_pong, fd2, 5};
  auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<int>();
  auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<int>();
  // Bridge setup.
  auto bridge = net::octet_stream::make_flow_bridge(16, 16, trait{},
                                                    std::move(s2a_pull),
                                                    std::move(a2s_push));
  auto transport = net::octet_stream::transport::make(fd1, std::move(bridge));
  transport->active_policy().connect();
  auto ptr = net::socket_manager::make(mpx.get(), std::move(transport));
  mpx->start(ptr);
  // Run the flow to completion.
  auto received = std::make_shared<std::vector<int>>();
  auto self = flow::scoped_coordinator::make();
  self->make_observable().never<int>().subscribe(std::move(s2a_push));
  a2s_pull.observe_on(self.get()).take(5).for_each([received](int item) {
    received->push_back(item);
  });
  self->run_some(std::chrono::seconds(1)); // Run at most for 1 second.
  // Check the results.
  auto want = std::vector<int>();
  want.resize(5);
  std::iota(want.begin(), want.end(), 2);
  check_eq(*received, want);
  // Cleanup.
  ping_pong_thread.join();
}

} // WITH_FIXTURE(fixture)

} // namespace
