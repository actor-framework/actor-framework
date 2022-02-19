// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE stream_transport

#include "caf/net/stream_transport.hpp"

#include "net-test.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/make_actor.hpp"
#include "caf/net/actor_proxy_impl.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace caf::net;

namespace {

constexpr string_view hello_manager = "hello manager!";

struct fixture : test_coordinator_fixture<>, host_fixture {
  using byte_buffer_ptr = std::shared_ptr<byte_buffer>;

  fixture()
    : mpx(nullptr),
      recv_buf(1024),
      shared_recv_buf{std::make_shared<byte_buffer>()},
      shared_send_buf{std::make_shared<byte_buffer>()} {
    mpx.set_thread_id();
    mpx.apply_updates();
    if (auto err = mpx.init())
      FAIL("mpx.init failed: " << err);
    REQUIRE_EQ(mpx.num_socket_managers(), 1u);
    auto sockets = unbox(make_stream_socket_pair());
    send_socket_guard.reset(sockets.first);
    recv_socket_guard.reset(sockets.second);
    if (auto err = nonblocking(recv_socket_guard.socket(), true))
      FAIL("nonblocking returned an error: " << err);
  }

  bool handle_io_event() override {
    return mpx.poll_once(false);
  }

  settings config;
  multiplexer mpx;
  byte_buffer recv_buf;
  socket_guard<stream_socket> send_socket_guard;
  socket_guard<stream_socket> recv_socket_guard;
  byte_buffer_ptr shared_recv_buf;
  byte_buffer_ptr shared_send_buf;
};

class dummy_application {
public:
  using byte_buffer_ptr = std::shared_ptr<byte_buffer>;

  using input_tag = tag::stream_oriented;

  explicit dummy_application(byte_buffer_ptr recv_buf, byte_buffer_ptr send_buf)
    : recv_buf_(std::move(recv_buf)),
      send_buf_(std::move(send_buf)){
        // nop
      };

  ~dummy_application() = default;

  template <class ParentPtr>
  error init(socket_manager*, ParentPtr parent, const settings&) {
    parent->configure_read(receive_policy::exactly(hello_manager.size()));
    return none;
  }

  template <class ParentPtr>
  bool prepare_send(ParentPtr parent) {
    MESSAGE("prepare_send called");
    auto& buf = parent->output_buffer();
    auto data = as_bytes(make_span(hello_manager));
    buf.insert(buf.end(), data.begin(), data.end());
    return true;
  }

  template <class ParentPtr>
  bool done_sending(ParentPtr) {
    MESSAGE("done_sending called");
    return true;
  }

  template <class ParentPtr>
  void continue_reading(ParentPtr) {
    FAIL("continue_reading called");
  }

  template <class ParentPtr>
  size_t consume(ParentPtr, span<const byte> data, span<const byte>) {
    recv_buf_->clear();
    recv_buf_->insert(recv_buf_->begin(), data.begin(), data.end());
    MESSAGE("Received " << recv_buf_->size() << " bytes in dummy_application");
    return recv_buf_->size();
  }

  static void handle_error(sec code) {
    FAIL("handle_error called with " << CAF_ARG(code));
  }

  template <class ParentPtr>
  static void abort(ParentPtr, const error& reason) {
    FAIL("abort called with " << CAF_ARG(reason));
  }

private:
  byte_buffer_ptr recv_buf_;
  byte_buffer_ptr send_buf_;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(receive) {
  auto mgr = make_socket_manager<dummy_application, stream_transport>(
    recv_socket_guard.release(), &mpx, shared_recv_buf, shared_send_buf);
  CHECK_EQ(mgr->init(config), none);
  mpx.apply_updates();
  CHECK_EQ(mpx.num_socket_managers(), 2u);
  CHECK_EQ(static_cast<size_t>(write(send_socket_guard.socket(),
                                     as_bytes(make_span(hello_manager)))),
           hello_manager.size());
  MESSAGE("wrote " << hello_manager.size() << " bytes.");
  run();
  CHECK_EQ(string_view(reinterpret_cast<char*>(shared_recv_buf->data()),
                       shared_recv_buf->size()),
           hello_manager);
}

CAF_TEST(send) {
  auto mgr = make_socket_manager<dummy_application, stream_transport>(
    recv_socket_guard.release(), &mpx, shared_recv_buf, shared_send_buf);
  CHECK_EQ(mgr->init(config), none);
  mpx.apply_updates();
  CHECK_EQ(mpx.num_socket_managers(), 2u);
  mgr->register_writing();
  mpx.apply_updates();
  while (handle_io_event())
    ;
  recv_buf.resize(hello_manager.size());
  auto res = read(send_socket_guard.socket(), make_span(recv_buf));
  MESSAGE("received " << res << " bytes");
  recv_buf.resize(res);
  CHECK_EQ(string_view(reinterpret_cast<char*>(recv_buf.data()),
                       recv_buf.size()),
           hello_manager);
}

END_FIXTURE_SCOPE()
