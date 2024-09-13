// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/octet_stream/transport.hpp"

#include "caf/test/test.hpp"

#include "caf/net/multiplexer.hpp"
#include "caf/net/receive_policy.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/log/test.hpp"
#include "caf/make_actor.hpp"
#include "caf/span.hpp"

#include <algorithm>

using namespace caf;

namespace os = net::octet_stream;

namespace {

constexpr std::string_view hello_manager = "hello manager!";

struct fixture {
  fixture() : mpx(net::multiplexer::make(nullptr)), send_buf(1024) {
    mpx->set_thread_id();
    mpx->apply_updates();
    if (auto err = mpx->init())
      CAF_RAISE_ERROR("mpx->init failed");
    if (mpx->num_socket_managers() != 1u)
      CAF_RAISE_ERROR("mpx->num_socket_managers() != 1u");
    auto maybe_sockets = net::make_stream_socket_pair();
    if (!maybe_sockets)
      CAF_RAISE_ERROR("failed to create socket pair");
    auto& sockets = *maybe_sockets;
    send_socket_guard.reset(sockets.first);
    recv_socket_guard.reset(sockets.second);
    if (auto err = nonblocking(recv_socket_guard.socket(), true))
      CAF_RAISE_ERROR("failed to set socket to nonblocking");
  }

  bool handle_io_event() {
    return mpx->poll_once(false);
  }

  settings config;
  net::multiplexer_ptr mpx;
  net::socket_guard<net::stream_socket> send_socket_guard;
  net::socket_guard<net::stream_socket> recv_socket_guard;
  byte_buffer recv_buf;
  byte_buffer send_buf;
};

class mock_application : public os::upper_layer {
public:
  using consume_impl_t = std::function<ptrdiff_t(byte_span, byte_span)>;

  explicit mock_application() = default;

  explicit mock_application(consume_impl_t consume_impl)
    : consume_impl_{std::move(consume_impl)} {
    // nop
  }

  static auto make() {
    return std::make_unique<mock_application>();
  }

  static auto make(consume_impl_t consume_impl) {
    return std::make_unique<mock_application>(std::move(consume_impl));
  }

  os::lower_layer* down;

  error start(os::lower_layer* down_ptr) override {
    down = down_ptr;
    down->configure_read(net::receive_policy::up_to(hello_manager.size()));
    return none;
  }

  void abort(const error&) override {
    CAF_RAISE_ERROR("abort called");
  }

  ptrdiff_t consume(byte_span data, byte_span delta) override {
    return consume_impl_(data, delta);
  }

  void prepare_send() override {
    log::test::debug("prepare_send called");
    auto& buf = down->output_buffer();
    auto data = as_bytes(make_span(hello_manager));
    buf.insert(buf.end(), data.begin(), data.end());
  }

  bool done_sending() override {
    log::test::debug("done_sending called");
    return true;
  }

  void consume_impl(consume_impl_t consume_impl) {
    consume_impl_ = std::move(consume_impl);
  }

private:
  consume_impl_t consume_impl_;
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("receive") {
  auto mock = mock_application::make([this](byte_span data, byte_span) {
    recv_buf.clear();
    recv_buf.insert(recv_buf.begin(), data.begin(), data.end());
    log::test::debug("Received {} bytes in mock_application", recv_buf.size());
    return static_cast<ptrdiff_t>(recv_buf.size());
  });
  auto transport = os::transport::make(recv_socket_guard.release(),
                                       std::move(mock));
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  check_eq(mgr->start(), none);
  mpx->apply_updates();
  check_eq(mpx->num_socket_managers(), 2u);
  check_eq(static_cast<size_t>(write(send_socket_guard.socket(),
                                     as_bytes(make_span(hello_manager)))),
           hello_manager.size());
  log::test::debug("wrote {} bytes.", hello_manager.size());
  handle_io_event();
  check_eq(std::string_view(reinterpret_cast<char*>(recv_buf.data()),
                            recv_buf.size()),
           hello_manager);
}

TEST("send") {
  auto mock = mock_application::make();
  auto transport = os::transport::make(recv_socket_guard.release(),
                                       std::move(mock));
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  check_eq(mgr->start(), none);
  mpx->apply_updates();
  check_eq(mpx->num_socket_managers(), 2u);
  mgr->register_writing();
  mpx->apply_updates();
  while (handle_io_event())
    ;
  send_buf.resize(hello_manager.size());
  auto res = read(send_socket_guard.socket(), make_span(send_buf));
  log::test::debug("received  bytes", res);
  send_buf.resize(res);
  check_eq(std::string_view(reinterpret_cast<char*>(send_buf.data()),
                            send_buf.size()),
           hello_manager);
}

TEST("consuming a non-negative byte count resets the delta") {
  std::vector<std::pair<size_t, size_t>> byte_span_sizes;
  auto mock = mock_application::make(
    [&byte_span_sizes](byte_span data, byte_span delta) {
      byte_span_sizes.emplace_back(data.size(), delta.size());
      // consume half the input and get called twice
      return std::min(ptrdiff_t{7}, static_cast<ptrdiff_t>(data.size()));
    });
  auto transport = os::transport::make(recv_socket_guard.release(),
                                       std::move(mock));
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  check_eq(mgr->start(), none);
  mpx->apply_updates();
  write(send_socket_guard.socket(), as_bytes(make_span(hello_manager)));
  handle_io_event();
  if (check_eq(byte_span_sizes.size(), 2u)) {
    check_eq(byte_span_sizes[0].first, 14u);
    check_eq(byte_span_sizes[0].second, 14u);
    check_eq(byte_span_sizes[1].first, 7u);
    check_eq(byte_span_sizes[1].second, 7u);
  }
}

TEST("switching the protocol resets the delta") {
  std::vector<std::pair<size_t, size_t>> byte_span_sizes_1;
  auto mock_1 = mock_application::make(
    [&byte_span_sizes_1](byte_span data, byte_span delta) {
      byte_span_sizes_1.emplace_back(data.size(), delta.size());
      // consume the whole of the input
      return static_cast<ptrdiff_t>(data.size());
    });
  std::vector<std::pair<size_t, size_t>> byte_span_sizes_2;
  auto mock_2 = mock_application::make();
  mock_2->consume_impl([&mock_1, &byte_span_sizes_2,
                        mock = mock_2.get()](byte_span data, byte_span delta) {
    byte_span_sizes_2.emplace_back(data.size(), delta.size());
    mock->down->switch_protocol(std::move(mock_1));
    // consume half the input, leave half for the next protocol
    return std::min(ptrdiff_t{7}, static_cast<ptrdiff_t>(data.size()));
  });
  auto transport = os::transport::make(recv_socket_guard.release(),
                                       std::move(mock_2));
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  check_eq(mgr->start(), none);
  mpx->apply_updates();
  write(send_socket_guard.socket(), as_bytes(make_span(hello_manager)));
  handle_io_event();
  if (check_eq(byte_span_sizes_1.size(), 1u)) {
    check_eq(byte_span_sizes_1[0].first, 7u);
    check_eq(byte_span_sizes_1[0].second, 7u);
  }
  if (check_eq(byte_span_sizes_2.size(), 1u)) {
    check_eq(byte_span_sizes_2[0].first, 14u);
    check_eq(byte_span_sizes_2[0].second, 14u);
  }
}

} // WITH_FIXTURE(fixture)
