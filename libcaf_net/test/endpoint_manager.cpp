// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE endpoint_manager

#include "caf/net/endpoint_manager.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/make_actor.hpp"
#include "caf/net/actor_proxy_impl.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/node_id.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace caf::net;

namespace {

using byte_buffer_ptr = std::shared_ptr<byte_buffer>;

string_view hello_manager{"hello manager!"};

string_view hello_test{"hello test!"};

struct fixture : test_coordinator_fixture<>, host_fixture {
  fixture() {
    mpx = std::make_shared<multiplexer>();
    mpx->set_thread_id();
    if (auto err = mpx->init())
      CAF_FAIL("mpx->init failed: " << err);
    if (mpx->num_socket_managers() != 1)
      CAF_FAIL("mpx->num_socket_managers() != 1");
  }

  bool handle_io_event() override {
    return mpx->poll_once(false);
  }

  multiplexer_ptr mpx;
};

class dummy_application {
  // nop
};

class dummy_transport {
public:
  using application_type = dummy_application;

  dummy_transport(stream_socket handle, byte_buffer_ptr data)
    : handle_(handle), data_(data), read_buf_(1024) {
    // nop
  }

  stream_socket handle() {
    return handle_;
  }

  template <class Manager>
  error init(Manager& manager) {
    auto test_bytes = as_bytes(make_span(hello_test));
    buf_.insert(buf_.end(), test_bytes.begin(), test_bytes.end());
    manager.register_writing();
    return none;
  }

  template <class Manager>
  bool handle_read_event(Manager&) {
    auto num_bytes = read(handle_, read_buf_);
    if (num_bytes > 0) {
      data_->insert(data_->end(), read_buf_.begin(),
                    read_buf_.begin() + num_bytes);
      return true;
    }
    return num_bytes < 0 && last_socket_error_is_temporary();
  }

  template <class Manager>
  bool handle_write_event(Manager& mgr) {
    for (auto x = mgr.next_message(); x != nullptr; x = mgr.next_message()) {
      binary_serializer sink{mgr.system(), buf_};
      if (auto err = sink(x->msg->payload))
        CAF_FAIL("serializing failed: " << err);
    }
    auto num_bytes = write(handle_, buf_);
    if (num_bytes > 0) {
      buf_.erase(buf_.begin(), buf_.begin() + num_bytes);
      return buf_.size() > 0;
    }
    return num_bytes < 0 && last_socket_error_is_temporary();
  }

  void handle_error(sec) {
    // nop
  }

  template <class Manager>
  void resolve(Manager& mgr, const uri& locator, const actor& listener) {
    actor_id aid = 42;
    auto hid = string_view("0011223344556677889900112233445566778899");
    auto nid = unbox(make_node_id(42, hid));
    actor_config cfg;
    auto p = make_actor<actor_proxy_impl, strong_actor_ptr>(
      aid, nid, &mgr.system(), cfg, &mgr);
    std::string path{locator.path().begin(), locator.path().end()};
    anon_send(listener, resolve_atom_v, std::move(path), p);
  }

  template <class Manager>
  void timeout(Manager&, const std::string&, uint64_t) {
    // nop
  }

  template <class Parent>
  void new_proxy(Parent&, const node_id&, actor_id) {
    // nop
  }

  template <class Parent>
  void local_actor_down(Parent&, const node_id&, actor_id, error) {
    // nop
  }

private:
  stream_socket handle_;

  byte_buffer_ptr data_;

  byte_buffer read_buf_;

  byte_buffer buf_;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(endpoint_manager_tests, fixture)

CAF_TEST(send and receive) {
  byte_buffer read_buf(1024);
  auto buf = std::make_shared<byte_buffer>();
  auto sockets = unbox(make_stream_socket_pair());
  CAF_CHECK_EQUAL(nonblocking(sockets.second, true), none);
  CAF_CHECK_LESS(read(sockets.second, read_buf), 0);
  CAF_CHECK(last_socket_error_is_temporary());
  auto guard = detail::make_scope_guard([&] { close(sockets.second); });
  auto mgr = make_endpoint_manager(mpx, sys,
                                   dummy_transport{sockets.first, buf});
  CAF_CHECK_EQUAL(mgr->mask(), operation::none);
  CAF_CHECK_EQUAL(mgr->init(), none);
  CAF_CHECK_EQUAL(mgr->mask(), operation::read_write);
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 2u);
  CAF_CHECK_EQUAL(write(sockets.second, as_bytes(make_span(hello_manager))),
                  hello_manager.size());
  run();
  CAF_CHECK_EQUAL(string_view(reinterpret_cast<char*>(buf->data()),
                              buf->size()),
                  hello_manager);
  CAF_CHECK_EQUAL(read(sockets.second, read_buf), hello_test.size());
  CAF_CHECK_EQUAL(string_view(reinterpret_cast<char*>(read_buf.data()),
                              hello_test.size()),
                  hello_test);
}

CAF_TEST(resolve and proxy communication) {
  byte_buffer read_buf(1024);
  auto buf = std::make_shared<byte_buffer>();
  auto sockets = unbox(make_stream_socket_pair());
  CAF_CHECK_EQUAL(nonblocking(sockets.second, true), none);
  auto guard = detail::make_scope_guard([&] { close(sockets.second); });
  auto mgr = make_endpoint_manager(mpx, sys,
                                   dummy_transport{sockets.first, buf});
  CAF_CHECK_EQUAL(mgr->init(), none);
  CAF_CHECK_EQUAL(mgr->mask(), operation::read_write);
  run();
  CAF_CHECK_EQUAL(read(sockets.second, read_buf), hello_test.size());
  mgr->resolve(unbox(make_uri("test:id/42")), self);
  run();
  self->receive(
    [&](resolve_atom, const std::string&, const strong_actor_ptr& p) {
      CAF_MESSAGE("got a proxy, send a message to it");
      self->send(actor_cast<actor>(p), "hello proxy!");
    },
    after(std::chrono::seconds(0)) >>
      [&] { CAF_FAIL("manager did not respond with a proxy."); });
  run();
  auto read_res = read(sockets.second, read_buf);
  if (read_res <= 0) {
    std::string msg = "socket closed";
    if (read_res < 0)
      msg = last_socket_error_as_string();
    CAF_ERROR("read() failed: " << msg);
    return;
  }
  read_buf.resize(static_cast<size_t>(read_res));
  CAF_MESSAGE("receive buffer contains " << read_buf.size() << " bytes");
  message msg;
  binary_deserializer source{sys, read_buf};
  CAF_CHECK_EQUAL(source(msg), none);
  if (msg.match_elements<std::string>())
    CAF_CHECK_EQUAL(msg.get_as<std::string>(0), "hello proxy!");
  else
    CAF_ERROR("expected a string, got: " << to_string(msg));
}

CAF_TEST_FIXTURE_SCOPE_END()
