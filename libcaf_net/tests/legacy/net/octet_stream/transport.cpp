// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.octet_stream.transport

#include "caf/net/octet_stream/transport.hpp"

#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/make_actor.hpp"
#include "caf/span.hpp"

#include "net-test.hpp"

using namespace caf;

namespace os = net::octet_stream;

namespace {

constexpr std::string_view hello_manager = "hello manager!";

struct fixture : test_coordinator_fixture<> {
  fixture() : mpx(net::multiplexer::make(nullptr)), send_buf(1024) {
    mpx->set_thread_id();
    mpx->apply_updates();
    if (auto err = mpx->init())
      FAIL("mpx->init failed: " << err);
    REQUIRE_EQ(mpx->num_socket_managers(), 1u);
    auto sockets = unbox(net::make_stream_socket_pair());
    send_socket_guard.reset(sockets.first);
    recv_socket_guard.reset(sockets.second);
    if (auto err = nonblocking(recv_socket_guard.socket(), true))
      FAIL("nonblocking returned an error: " << err);
  }

  bool handle_io_event() override {
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

  void abort(const error& reason) override {
    FAIL("abort called: " << CAF_ARG(reason));
  }

  ptrdiff_t consume(byte_span data, byte_span delta) override {
    return consume_impl_(data, delta);
  }

  void prepare_send() override {
    MESSAGE("prepare_send called");
    auto& buf = down->output_buffer();
    auto data = as_bytes(make_span(hello_manager));
    buf.insert(buf.end(), data.begin(), data.end());
  }

  bool done_sending() override {
    MESSAGE("done_sending called");
    return true;
  }

  void consume_impl(consume_impl_t consume_impl) {
    consume_impl_ = std::move(consume_impl);
  }

private:
  consume_impl_t consume_impl_;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(receive) {
  auto mock = mock_application::make([this](byte_span data, byte_span) {
    recv_buf.clear();
    recv_buf.insert(recv_buf.begin(), data.begin(), data.end());
    MESSAGE("Received " << recv_buf.size() << " bytes in mock_application");
    return static_cast<ptrdiff_t>(recv_buf.size());
  });
  auto transport = os::transport::make(recv_socket_guard.release(),
                                       std::move(mock));
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  CHECK_EQ(mgr->start(), none);
  mpx->apply_updates();
  CHECK_EQ(mpx->num_socket_managers(), 2u);
  CHECK_EQ(static_cast<size_t>(write(send_socket_guard.socket(),
                                     as_bytes(make_span(hello_manager)))),
           hello_manager.size());
  MESSAGE("wrote " << hello_manager.size() << " bytes.");
  run();
  CHECK_EQ(std::string_view(reinterpret_cast<char*>(recv_buf.data()),
                            recv_buf.size()),
           hello_manager);
}

CAF_TEST(send) {
  auto mock = mock_application::make();
  auto transport = os::transport::make(recv_socket_guard.release(),
                                       std::move(mock));
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  CHECK_EQ(mgr->start(), none);
  mpx->apply_updates();
  CHECK_EQ(mpx->num_socket_managers(), 2u);
  mgr->register_writing();
  mpx->apply_updates();
  while (handle_io_event())
    ;
  send_buf.resize(hello_manager.size());
  auto res = read(send_socket_guard.socket(), make_span(send_buf));
  MESSAGE("received " << res << " bytes");
  send_buf.resize(res);
  CHECK_EQ(std::string_view(reinterpret_cast<char*>(send_buf.data()),
                            send_buf.size()),
           hello_manager);
}

CAF_TEST(consuming a non - negative byte count resets the delta) {
  std::vector<std::pair<size_t, size_t>> byte_span_sizes;
  auto mock = mock_application::make(
    [&byte_span_sizes](byte_span data, byte_span delta) {
      byte_span_sizes.emplace_back(data.size(), delta.size());
      // consume half the input and get called twice
      return std::min(7l, static_cast<ptrdiff_t>(data.size()));
    });
  auto transport = os::transport::make(recv_socket_guard.release(),
                                       std::move(mock));
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  CHECK_EQ(mgr->start(), none);
  mpx->apply_updates();
  write(send_socket_guard.socket(), as_bytes(make_span(hello_manager)));
  run();
  if (CHECK_EQ(byte_span_sizes.size(), 2u)) {
    CHECK_EQ(byte_span_sizes[0].first, 14u);
    CHECK_EQ(byte_span_sizes[0].second, 14u);
    CHECK_EQ(byte_span_sizes[1].first, 7u);
    CHECK_EQ(byte_span_sizes[1].second, 7u);
  }
}

CAF_TEST(switching the protocol resets the delta) {
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
    return std::min(7l, static_cast<ptrdiff_t>(data.size()));
  });
  auto transport = os::transport::make(recv_socket_guard.release(),
                                       std::move(mock_2));
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  CHECK_EQ(mgr->start(), none);
  mpx->apply_updates();
  write(send_socket_guard.socket(), as_bytes(make_span(hello_manager)));
  run();
  if (CHECK_EQ(byte_span_sizes_1.size(), 1u)) {
    CHECK_EQ(byte_span_sizes_1[0].first, 7u);
    CHECK_EQ(byte_span_sizes_1[0].second, 7u);
  }
  if (CHECK_EQ(byte_span_sizes_2.size(), 1u)) {
    CHECK_EQ(byte_span_sizes_2[0].first, 14u);
    CHECK_EQ(byte_span_sizes_2[0].second, 14u);
  }
}

END_FIXTURE_SCOPE()
