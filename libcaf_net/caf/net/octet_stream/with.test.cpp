// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/octet_stream/with.hpp"

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

// Note: the test here are similar to flow_bridge.test.cpp, except that they use
//       the `with` DSL instead of the `flow_bridge` class directly.

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

} // namespace

WITH_FIXTURE(fixture) {

TEST("a flow bridge connects flows to a socket") {
  // Socket and thread setup.
  auto fd_pair = net::make_stream_socket_pair();
  require(fd_pair.has_value());
  auto [fd1, fd2] = *fd_pair;
  auto ping_pong_thread = std::thread{ping_pong, fd2, 50};
  auto worker = std::thread{};
  // Bridge setup.
  auto received = std::make_shared<std::vector<int>>();
  auto rendezvous = std::make_shared<detail::latch>(2);
  auto start_res
    = net::octet_stream::with(mpx.get())
        .connect(fd1)
        .read_buffer_size(16)
        .write_buffer_size(16)
        .start([received, &rendezvous, &worker](auto pull, auto push) {
          worker = std::thread{[pull, push, received, rendezvous] {
            auto self = flow::scoped_coordinator::make();
            self->make_observable()
              .iota(1) //
              .take(50)
              .map([](int x) { return std::byte{static_cast<uint8_t>(x)}; })
              .subscribe(push);
            pull.observe_on(self.get())
              .map([](std::byte item) { return std::to_integer<int>(item); })
              .do_finally([rendezvous] { rendezvous->count_down(); })
              .for_each([received](int item) { received->push_back(item); });
            self->run();
          }};
        });
  require(start_res.has_value());
  // Check the results.
  rendezvous->count_down_and_wait();
  auto want = std::vector<int>();
  want.resize(50);
  std::iota(want.begin(), want.end(), 2);
  check_eq(*received, want);
  // Cleanup.
  ping_pong_thread.join();
  worker.join();
}

} // WITH_FIXTURE(fixture)
