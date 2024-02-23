// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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

using net::octet_stream::flow_bridge;

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

// Send `num_items` bytes to the given socket.
void iota_send(net::stream_socket fd, int num_items) {
  auto guard = detail::scope_guard{[fd]() noexcept { net::close(fd); }};
  for (int i = 1; i <= num_items; ++i) {
    std::byte buf{static_cast<uint8_t>(i % 256)};
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

TEST("a flow bridge connects flows to a socket") {
  // Socket and thread setup.
  auto fd_pair = net::make_stream_socket_pair();
  require(fd_pair.has_value());
  auto [fd1, fd2] = *fd_pair;
  auto ping_pong_thread = std::thread{ping_pong, fd2, 50};
  // Bridge setup.
  auto received = std::make_shared<std::vector<int>>();
  auto rendezvous = detail::latch{2};
  auto start_res
    = net::octet_stream::with(mpx.get())
        .connect(fd1)
        .read_buffer_size(16)
        .write_buffer_size(16)
        .send_from([](flow::coordinator* self) {
          return self->make_observable()
            .iota(1) //
            .take(50)
            .map([](int x) { return std::byte{static_cast<uint8_t>(x)}; });
        })
        .start([received, &rendezvous](flow::observable<std::byte> bytes) {
          bytes.map([](std::byte item) { return std::to_integer<int>(item); })
            .do_finally([&rendezvous] { rendezvous.count_down(); })
            .for_each([received](int item) { received->push_back(item); });
        });
  require(start_res.has_value());
  // Check the results.
  rendezvous.count_down_and_wait();
  auto want = std::vector<int>();
  want.resize(50);
  std::iota(want.begin(), want.end(), 2);
  check_eq(*received, want);
  // Cleanup.
  ping_pong_thread.join();
  // Must be closed by now.
  auto self = flow::scoped_coordinator::make();
  auto closed = std::make_shared<bool>(false);
  auto err = std::make_shared<error>();
  start_res->first.observe_on(self.get())
    .do_on_complete([closed] { *closed = true; })
    .do_on_error([closed, err](error x) {
      *closed = true;
      *err = std::move(x);
    })
    .for_each([received](unit_t) {});
  self->run();
  check(*closed);
  check_eq(*err, sec::socket_disconnected);
}

TEST("a bridge can make received data available through a publisher") {
  // Socket and thread setup.
  auto fd_pair = net::make_stream_socket_pair();
  require(fd_pair.has_value());
  auto [fd1, fd2] = *fd_pair;
  auto ping_pong_thread = std::thread{ping_pong, fd2, 50};
  // Bridge setup.
  auto start_res
    = net::octet_stream::with(mpx.get())
        .connect(fd1)
        .read_buffer_size(16)
        .write_buffer_size(16)
        .send_from([](flow::coordinator* self) {
          return self->make_observable()
            .iota(1) //
            .take(50)
            .map([](int x) { return std::byte{static_cast<uint8_t>(x)}; });
        })
        .start([](flow::observable<std::byte> bytes) {
          return bytes
            .map([](std::byte item) { return std::to_integer<int>(item); })
            .share(1);
        });
  require(start_res.has_value());
  auto res = start_res->first;
  // Receive the results. We wait a bit here to subscribe after the bridge has
  // been fully set up in order to trigger more code paths.
  std::this_thread::sleep_for(10ms);
  auto received = std::make_shared<std::vector<int>>();
  auto self = flow::scoped_coordinator::make();
  res.observe_on(self.get()).for_each([received](int item) {
    received->push_back(item);
  });
  self->run();
  // Check the results.
  auto want = std::vector<int>();
  want.resize(50);
  std::iota(want.begin(), want.end(), 2);
  check_eq(*received, want);
  // Cleanup.
  ping_pong_thread.join();
}

TEST("a bridge may use time-based flow operators") {
  // Socket and thread setup.
  auto fd_pair = net::make_stream_socket_pair();
  require(fd_pair.has_value());
  auto [fd1, fd2] = *fd_pair;
  auto ping_pong_thread = std::thread{ping_pong, fd2, 5};
  // Bridge setup.
  auto received = std::make_shared<std::vector<int>>();
  auto rendezvous = detail::latch{2};
  auto start_res
    = net::octet_stream::with(mpx.get())
        .connect(fd1)
        .read_buffer_size(16)
        .write_buffer_size(16)
        .send_from([](flow::coordinator* self) {
          return self->make_observable()
            .interval(10ms) //
            .take(5)
            .map([](int64_t x) { return std::byte{static_cast<uint8_t>(x)}; });
        })
        .start([received, &rendezvous](flow::observable<std::byte> bytes) {
          bytes.map([](std::byte item) { return std::to_integer<int>(item); })
            .do_finally([&rendezvous] { rendezvous.count_down(); })
            .for_each([received](int item) { received->push_back(item); });
        });
  require(start_res.has_value());
  // Check the results.
  rendezvous.count_down_and_wait();
  auto want = std::vector<int>();
  want.resize(5);
  std::iota(want.begin(), want.end(), 1);
  check_eq(*received, want);
  // Cleanup.
  ping_pong_thread.join();
}

TEST("omitting send_from produces no output on the socket") {
  // Socket and thread setup.
  auto fd_pair = net::make_stream_socket_pair();
  require(fd_pair.has_value());
  auto [fd1, fd2] = *fd_pair;
  auto iota_thread = std::thread{iota_send, fd2, 1024};
  // Bridge setup.
  auto received_total = std::make_shared<size_t>();
  auto rendezvous = detail::latch{2};
  auto start_res
    = net::octet_stream::with(mpx.get())
        .connect(fd1)
        .read_buffer_size(16)
        .write_buffer_size(16)
        .start(
          [received_total, &rendezvous](flow::observable<std::byte> bytes) {
            bytes.map([](std::byte item) { return std::to_integer<int>(item); })
              .do_finally([&rendezvous] { rendezvous.count_down(); })
              .for_each([received_total](int) { *received_total += 1; });
          });
  require(start_res.has_value());
  // Check the results.
  rendezvous.count_down_and_wait();
  check_eq(*received_total, 1024u);
  // Cleanup.
  iota_thread.join();
}

TEST("subscribing std::ignore to the inputs discards received data") {
  // Socket and thread setup.
  auto fd_pair = net::make_stream_socket_pair();
  require(fd_pair.has_value());
  auto [fd1, fd2] = *fd_pair;
  auto ping_pong_thread = std::thread{iota_send, fd2, 1024};
  // Bridge setup.
  auto received_total = std::make_shared<size_t>();
  auto rendezvous = detail::latch{2};
  auto start_res
    = net::octet_stream::with(mpx.get())
        .connect(fd1)
        .read_buffer_size(16)
        .write_buffer_size(16)
        .start(
          [received_total, &rendezvous](flow::observable<std::byte> bytes) {
            bytes.map([](std::byte item) { return std::to_integer<int>(item); })
              .do_finally([&rendezvous] { rendezvous.count_down(); })
              .do_on_next([received_total](int) { *received_total += 1; })
              .subscribe(std::ignore);
          });
  require(start_res.has_value());
  // Check the results.
  rendezvous.count_down_and_wait();
  check_eq(*received_total, 1024u);
  // Cleanup.
  ping_pong_thread.join();
}

} // WITH_FIXTURE(fixture)

} // namespace
