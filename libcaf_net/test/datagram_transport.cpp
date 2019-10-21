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

#define CAF_SUITE datagram_transport

#include "caf/net/datagram_transport.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

#include "caf/byte.hpp"
#include "caf/make_actor.hpp"
#include "caf/net/actor_proxy_impl.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/udp_datagram_socket.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace caf::net;

namespace {

constexpr string_view hello_manager = "hello manager!";

class dummy_application_factory;

struct fixture : test_coordinator_fixture<>, host_fixture {
  fixture() {
    mpx = std::make_shared<multiplexer>();
    if (auto err = mpx->init())
      CAF_FAIL("mpx->init failed: " << sys.render(err));
  }

  bool handle_io_event() override {
    return mpx->poll_once(false);
  }

  multiplexer_ptr mpx;
};

class dummy_application {
public:
  using transport_type = datagram_transport<dummy_application_factory>;

  dummy_application(std::shared_ptr<std::vector<byte>> rec_buf)
    : rec_buf_(std::move(rec_buf)){
      // nop
    };

  ~dummy_application() = default;

  template <class Parent>
  error init(Parent&) {
    return none;
  }

  template <class Transport>
  void write_message(Transport& transport,
                     std::unique_ptr<endpoint_manager_queue::message> msg) {
    transport.write_packet(span<byte>{}, msg->payload);
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
    auto hid = "0011223344556677889900112233445566778899";
    auto nid = unbox(make_node_id(42, hid));
    actor_config cfg;
    endpoint_manager_ptr ptr{&parent.manager()};
    auto p = make_actor<actor_proxy_impl, strong_actor_ptr>(aid, nid,
                                                            &parent.system(),
                                                            cfg,
                                                            std::move(ptr));
    anon_send(listener, resolve_atom::value, path, p);
  }

  template <class Parent>
  void new_proxy(Parent&, actor_id) {
    // nop
  }

  template <class Parent>
  void local_actor_down(Parent&, actor_id, error) {
    // nop
  }

  template <class Transport>
  void timeout(Transport&, atom_value, uint64_t) {
    // nop
  }

  void handle_error(sec) {
    // nop
  }

  static expected<std::vector<byte>> serialize(actor_system& sys,
                                               const type_erased_tuple& x) {
    std::vector<byte> result;
    serializer_impl<std::vector<byte>> sink{sys, result};
    if (auto err = message::save(sink, x))
      return err;
    return result;
  }

private:
  std::shared_ptr<std::vector<byte>> rec_buf_;
};

class dummy_application_factory {
public:
  using application_type = dummy_application;

  explicit dummy_application_factory(std::shared_ptr<std::vector<byte>> buf)
    : buf_(std::move(buf)) {
    // nop
  }

  dummy_application make() {
    return dummy_application{buf_};
  }

private:
  std::shared_ptr<std::vector<byte>> buf_;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(datagram_transport_tests, fixture)

CAF_TEST(receive) {
  using transport_type = datagram_transport<dummy_application_factory>;
  auto buf = std::make_shared<std::vector<byte>>();
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 1u);
  ip_endpoint ep;
  if (auto err = parse("127.0.0.1:0", ep))
    CAF_FAIL("parse returned an error: " << err);
  auto send_pair = unbox(make_udp_datagram_socket(ep));
  auto sender = send_pair.first;
  auto receive_pair = unbox(make_udp_datagram_socket(ep));
  auto receiver = receive_pair.first;
  ep.port(htons(receive_pair.second));
  auto send_guard = make_socket_guard(sender);
  auto receive_guard = make_socket_guard(receiver);
  if (auto err = nonblocking(receiver, true))
    CAF_FAIL("nonblocking() returned an error: " << err);
  transport_type transport{receiver, dummy_application_factory{buf}};
  transport.configure_read(net::receive_policy::exactly(hello_manager.size()));
  auto mgr = make_endpoint_manager(mpx, sys, transport);
  CAF_CHECK_EQUAL(mgr->init(), none);
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 2u);
  CAF_CHECK_EQUAL(write(sender, as_bytes(make_span(hello_manager)), ep),
                  hello_manager.size());
  CAF_MESSAGE("wrote " << hello_manager.size() << " bytes.");
  run();
  CAF_CHECK_EQUAL(string_view(reinterpret_cast<char*>(buf->data()),
                              buf->size()),
                  hello_manager);
}

// TODO: test is disabled until resolve in transport_worker_dispatcher is
// implemented correctly.
/*
CAF_TEST(resolve and proxy communication) {
  using transport_type = datagram_transport<dummy_application_factory>;
  auto buf = std::make_shared<std::vector<byte>>();
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 1u);
  ip_endpoint ep;
  if (auto err = parse("127.0.0.1:0", ep))
    CAF_FAIL("parse returned an error: " << err);
  auto sender = unbox(make_udp_datagram_socket(ep));
  ep.port(0);
  auto receiver = unbox(make_udp_datagram_socket(ep));
  auto send_guard = make_socket_guard(sender);
  auto receive_guard = make_socket_guard(receiver);
  if (auto err = nonblocking(receiver, true))
    CAF_FAIL("nonblocking() returned an error: " << err);
  auto test_read_res = read(receiver, make_span(*buf));
  if (auto p = get_if<std::pair<size_t, ip_endpoint>>(&test_read_res))
    CAF_CHECK_EQUAL(p->first, 0u);
  else
    CAF_FAIL("read returned an error: " << get<sec>(test_read_res));
  auto mgr = make_endpoint_manager(mpx, sys,
                                   transport_type{sender,
                                                  dummy_application_factory{
                                                    buf}});
  CAF_CHECK_EQUAL(mgr->init(), none);
  mpx->handle_updates();
  run();
  mgr->resolve("/id/42", self);
  run();
  self->receive(
    [&](resolve_atom, const std::string&, const strong_actor_ptr& p) {
      CAF_MESSAGE("got a proxy, send a message to it");
      self->send(actor_cast<actor>(p), "hello proxy!");
    },
    after(std::chrono::seconds(0)) >>
      [&] { CAF_FAIL("manager did not respond with a proxy."); });
  run();
  auto read_res = read(receiver, make_span(*buf));
  if (!holds_alternative<std::pair<size_t, ip_endpoint>>(read_res))
    CAF_FAIL("read() returned an error: " << sys.render(get<sec>(read_res)));
  buf->resize(get<std::pair<size_t, ip_endpoint>>(read_res).first);
  CAF_MESSAGE("receive buffer contains " << buf->size() << " bytes");
  message msg;
  binary_deserializer source{sys, *buf};
  CAF_CHECK_EQUAL(source(msg), none);
  if (msg.match_elements<std::string>())
    CAF_CHECK_EQUAL(msg.get_as<std::string>(0), "hello proxy!");
  else
    CAF_ERROR("expected a string, got: " << to_string(msg));
}
*/

CAF_TEST_FIXTURE_SCOPE_END()
