// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.stream_transport

#include "caf/net/stream_transport.hpp"

#include "net-test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/make_actor.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/span.hpp"

using namespace caf;

namespace {

constexpr std::string_view hello_manager = "hello manager!";

struct fixture : test_coordinator_fixture<> {
  using byte_buffer_ptr = std::shared_ptr<byte_buffer>;

  fixture()
    : mpx(net::multiplexer::make(nullptr)),
      recv_buf(1024),
      shared_recv_buf{std::make_shared<byte_buffer>()},
      shared_send_buf{std::make_shared<byte_buffer>()} {
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
  byte_buffer recv_buf;
  net::socket_guard<net::stream_socket> send_socket_guard;
  net::socket_guard<net::stream_socket> recv_socket_guard;
  byte_buffer_ptr shared_recv_buf;
  byte_buffer_ptr shared_send_buf;
};

class mock_application : public net::stream_oriented::upper_layer {
public:
  using byte_buffer_ptr = std::shared_ptr<byte_buffer>;

  explicit mock_application(byte_buffer_ptr recv_buf, byte_buffer_ptr send_buf)
    : recv_buf_(std::move(recv_buf)), send_buf_(std::move(send_buf)) {
    // nop
  }

  static auto make(byte_buffer_ptr recv_buf, byte_buffer_ptr send_buf) {
    return std::make_unique<mock_application>(std::move(recv_buf),
                                              std::move(send_buf));
  }

  net::stream_oriented::lower_layer* down;

  error start(net::stream_oriented::lower_layer* down_ptr) override {
    down = down_ptr;
    down->configure_read(net::receive_policy::exactly(hello_manager.size()));
    return none;
  }

  void abort(const error& reason) override {
    FAIL("abort called: " << CAF_ARG(reason));
  }

  ptrdiff_t consume(byte_span data, byte_span) override {
    recv_buf_->clear();
    recv_buf_->insert(recv_buf_->begin(), data.begin(), data.end());
    MESSAGE("Received " << recv_buf_->size() << " bytes in mock_application");
    return static_cast<ptrdiff_t>(recv_buf_->size());
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

private:
  byte_buffer_ptr recv_buf_;
  byte_buffer_ptr send_buf_;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(receive) {
  auto mock = mock_application::make(shared_recv_buf, shared_send_buf);
  auto transport = net::stream_transport::make(recv_socket_guard.release(),
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
  CHECK_EQ(std::string_view(reinterpret_cast<char*>(shared_recv_buf->data()),
                            shared_recv_buf->size()),
           hello_manager);
}

CAF_TEST(send) {
  auto mock = mock_application::make(shared_recv_buf, shared_send_buf);
  auto transport = net::stream_transport::make(recv_socket_guard.release(),
                                               std::move(mock));
  auto mgr = net::socket_manager::make(mpx.get(), std::move(transport));
  CHECK_EQ(mgr->start(), none);
  mpx->apply_updates();
  CHECK_EQ(mpx->num_socket_managers(), 2u);
  mgr->register_writing();
  mpx->apply_updates();
  while (handle_io_event())
    ;
  recv_buf.resize(hello_manager.size());
  auto res = read(send_socket_guard.socket(), make_span(recv_buf));
  MESSAGE("received " << res << " bytes");
  recv_buf.resize(res);
  CHECK_EQ(std::string_view(reinterpret_cast<char*>(recv_buf.data()),
                            recv_buf.size()),
           hello_manager);
}

END_FIXTURE_SCOPE()
