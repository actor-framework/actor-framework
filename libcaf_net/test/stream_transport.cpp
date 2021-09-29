// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE stream_transport

#include "caf/net/stream_transport.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

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
    if (auto err = mpx.init())
      CAF_FAIL("mpx.init failed: " << err);
    mpx.set_thread_id();
    CAF_CHECK_EQUAL(mpx.num_socket_managers(), 1u);
    auto sockets = unbox(make_stream_socket_pair());
    send_socket_guard.reset(sockets.first);
    recv_socket_guard.reset(sockets.second);
    if (auto err = nonblocking(recv_socket_guard.socket(), true))
      CAF_FAIL("nonblocking returned an error: " << err);
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
    CAF_MESSAGE("prepare_send called");
    auto& buf = parent->output_buffer();
    auto data = as_bytes(make_span(hello_manager));
    buf.insert(buf.end(), data.begin(), data.end());
    return true;
  }

  template <class ParentPtr>
  bool done_sending(ParentPtr) {
    CAF_MESSAGE("done_sending called");
    return true;
  }

  template <class ParentPtr>
  void continue_reading(ParentPtr) {
    CAF_FAIL("continue_reading called");
  }

  template <class ParentPtr>
  size_t consume(ParentPtr, span<const byte> data, span<const byte>) {
    recv_buf_->clear();
    recv_buf_->insert(recv_buf_->begin(), data.begin(), data.end());
    CAF_MESSAGE("Received " << recv_buf_->size()
                            << " bytes in dummy_application");
    return recv_buf_->size();
  }

  template <class ParentPtr>
  void resolve(ParentPtr parent, string_view path, const actor& listener) {
    actor_id aid = 42;
    auto hid = string_view("0011223344556677889900112233445566778899");
    auto nid = unbox(make_node_id(42, hid));
    actor_config cfg;
    endpoint_manager_ptr ptr{&parent->manager()};
    auto p = make_actor<actor_proxy_impl, strong_actor_ptr>(
      aid, nid, &parent->system(), cfg, std::move(ptr));
    anon_send(listener, resolve_atom_v, std::string{path.begin(), path.end()},
              p);
  }

  static void handle_error(sec code) {
    CAF_FAIL("handle_error called with " << CAF_ARG(code));
  }

  template <class ParentPtr>
  static void abort(ParentPtr, const error& reason) {
    CAF_FAIL("abort called with " << CAF_ARG(reason));
  }

private:
  byte_buffer_ptr recv_buf_;
  byte_buffer_ptr send_buf_;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(endpoint_manager_tests, fixture)

CAF_TEST(receive) {
  auto mgr = make_socket_manager<dummy_application, stream_transport>(
    recv_socket_guard.release(), &mpx, shared_recv_buf, shared_send_buf);
  CAF_CHECK_EQUAL(mgr->init(config), none);
  CAF_CHECK_EQUAL(mpx.num_socket_managers(), 2u);
  CAF_CHECK_EQUAL(static_cast<size_t>(
                    write(send_socket_guard.socket(),
                          as_bytes(make_span(hello_manager)))),
                  hello_manager.size());
  CAF_MESSAGE("wrote " << hello_manager.size() << " bytes.");
  run();
  CAF_CHECK_EQUAL(string_view(reinterpret_cast<char*>(shared_recv_buf->data()),
                              shared_recv_buf->size()),
                  hello_manager);
}

CAF_TEST(send) {
  auto mgr = make_socket_manager<dummy_application, stream_transport>(
    recv_socket_guard.release(), &mpx, shared_recv_buf, shared_send_buf);
  CAF_CHECK_EQUAL(mgr->init(config), none);
  CAF_CHECK_EQUAL(mpx.num_socket_managers(), 2u);
  mgr->register_writing();
  while (handle_io_event())
    ;
  recv_buf.resize(hello_manager.size());
  auto res = read(send_socket_guard.socket(), make_span(recv_buf));
  CAF_MESSAGE("received " << res << " bytes");
  recv_buf.resize(res);
  CAF_CHECK_EQUAL(string_view(reinterpret_cast<char*>(recv_buf.data()),
                              recv_buf.size()),
                  hello_manager);
}

CAF_TEST_FIXTURE_SCOPE_END()
