/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE stream_transport

#include "caf/net/stream_transport.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/make_actor.hpp"
#include "caf/net/actor_proxy_impl.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/endpoint_manager_impl.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace caf::net;

namespace {
constexpr string_view hello_manager = "hello manager!";

struct fixture : test_coordinator_fixture<>, host_fixture {
  using buffer_type = std::vector<byte>;

  using buffer_ptr = std::shared_ptr<buffer_type>;

  fixture() : recv_buf(1024), shared_buf{std::make_shared<buffer_type>()} {
    mpx = std::make_shared<multiplexer>();
    if (auto err = mpx->init())
      CAF_FAIL("mpx->init failed: " << err);
    mpx->set_thread_id();
    CAF_CHECK_EQUAL(mpx->num_socket_managers(), 1u);
    auto sockets = unbox(make_stream_socket_pair());
    send_socket_guard.reset(sockets.first);
    recv_socket_guard.reset(sockets.second);
    if (auto err = nonblocking(recv_socket_guard.socket(), true))
      CAF_FAIL("nonblocking returned an error: " << err);
  }

  bool handle_io_event() override {
    return mpx->poll_once(false);
  }

  multiplexer_ptr mpx;
  buffer_type recv_buf;
  socket_guard<stream_socket> send_socket_guard;
  socket_guard<stream_socket> recv_socket_guard;
  buffer_ptr shared_buf;
};

class dummy_application {
  using buffer_type = std::vector<byte>;

  using buffer_ptr = std::shared_ptr<buffer_type>;

public:
  dummy_application(buffer_ptr rec_buf)
    : rec_buf_(std::move(rec_buf)){
      // nop
    };

  ~dummy_application() = default;

  template <class Parent>
  error init(Parent&) {
    return none;
  }

  template <class Parent>
  void write_message(Parent& parent,
                     std::unique_ptr<endpoint_manager_queue::message> ptr) {
    parent.write_packet(ptr->payload);
  }

  template <class Parent>
  error handle_data(Parent&, span<const byte> data) {
    rec_buf_->clear();
    rec_buf_->insert(rec_buf_->begin(), data.begin(), data.end());
    return none;
  }

  template <class Parent>
  void resolve(Parent& parent, string_view path, const actor& listener) {
    actor_id aid = 42;
    auto hid = string_view("0011223344556677889900112233445566778899");
    auto nid = unbox(make_node_id(42, hid));
    actor_config cfg;
    endpoint_manager_ptr ptr{&parent.manager()};
    auto p = make_actor<actor_proxy_impl, strong_actor_ptr>(aid, nid,
                                                            &parent.system(),
                                                            cfg,
                                                            std::move(ptr));
    anon_send(listener, resolve_atom_v, std::string{path.begin(), path.end()},
              p);
  }

  template <class Parent>
  void timeout(Parent&, const std::string&, uint64_t) {
    // nop
  }

  template <class Parent>
  void new_proxy(Parent&, actor_id) {
    // nop
  }

  template <class Parent>
  void local_actor_down(Parent&, actor_id, error) {
    // nop
  }

  void handle_error(sec) {
    // nop
  }

  static expected<buffer_type> serialize(actor_system& sys, const message& x) {
    buffer_type result;
    binary_serializer sink{sys, result};
    if (auto err = x.save(sink))
      return err.value();
    return result;
  }

private:
  buffer_ptr rec_buf_;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(endpoint_manager_tests, fixture)

CAF_TEST(receive) {
  using transport_type = stream_transport<dummy_application>;
  auto mgr = make_endpoint_manager(mpx, sys,
                                   transport_type{recv_socket_guard.release(),
                                                  dummy_application{
                                                    shared_buf}});
  CAF_CHECK_EQUAL(mgr->init(), none);
  auto mgr_impl = mgr.downcast<endpoint_manager_impl<transport_type>>();
  CAF_CHECK(mgr_impl != nullptr);
  auto& transport = mgr_impl->transport();
  transport.configure_read(receive_policy::exactly(hello_manager.size()));
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 2u);
  CAF_CHECK_EQUAL(write(send_socket_guard.socket(),
                        as_bytes(make_span(hello_manager))),
                  hello_manager.size());
  CAF_MESSAGE("wrote " << hello_manager.size() << " bytes.");
  run();
  CAF_CHECK_EQUAL(string_view(reinterpret_cast<char*>(shared_buf->data()),
                              shared_buf->size()),
                  hello_manager);
}

CAF_TEST(resolve and proxy communication) {
  using transport_type = stream_transport<dummy_application>;
  auto mgr = make_endpoint_manager(mpx, sys,
                                   transport_type{send_socket_guard.release(),
                                                  dummy_application{
                                                    shared_buf}});
  CAF_CHECK_EQUAL(mgr->init(), none);
  run();
  mgr->resolve(unbox(make_uri("test:/id/42")), self);
  run();
  self->receive(
    [&](resolve_atom, const std::string&, const strong_actor_ptr& p) {
      CAF_MESSAGE("got a proxy, send a message to it");
      self->send(actor_cast<actor>(p), "hello proxy!");
    },
    after(std::chrono::seconds(0)) >>
      [&] { CAF_FAIL("manager did not respond with a proxy."); });
  run();
  auto read_res = read(recv_socket_guard.socket(), recv_buf);
  if (!holds_alternative<size_t>(read_res))
    CAF_FAIL("read() returned an error: " << get<sec>(read_res));
  recv_buf.resize(get<size_t>(read_res));
  CAF_MESSAGE("receive buffer contains " << recv_buf.size() << " bytes");
  message msg;
  binary_deserializer source{sys, recv_buf};
  CAF_CHECK_EQUAL(source(msg), none);
  if (msg.match_elements<std::string>())
    CAF_CHECK_EQUAL(msg.get_as<std::string>(0), "hello proxy!");
  else
    CAF_ERROR("expected a string, got: " << to_string(msg));
}

CAF_TEST_FIXTURE_SCOPE_END()
