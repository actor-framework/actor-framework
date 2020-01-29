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

#define CAF_SUITE string_application

#include "caf/net/stream_transport.hpp"

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

#include <vector>

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/byte.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/make_actor.hpp"
#include "caf/net/actor_proxy_impl.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/span.hpp"

using namespace caf;
using namespace caf::net;
using namespace caf::policy;

namespace {

using buffer_type = std::vector<byte>;

struct fixture : test_coordinator_fixture<>, host_fixture {
  fixture() {
    mpx = std::make_shared<multiplexer>();
    if (auto err = mpx->init())
      CAF_FAIL("mpx->init failed: " << sys.render(err));
    mpx->set_thread_id();
  }

  bool handle_io_event() override {
    return mpx->poll_once(false);
  }

  multiplexer_ptr mpx;
};

struct string_application_header {
  uint32_t payload;
};

/// @relates header
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f,
                                        string_application_header& hdr) {
  return f(meta::type_name("sa_header"), hdr.payload);
}

class string_application {
public:
  using header_type = string_application_header;

  string_application(actor_system& sys, std::shared_ptr<std::vector<byte>> buf)
    : sys_(sys), buf_(std::move(buf)) {
    // nop
  }

  template <class Parent>
  error init(Parent&) {
    return none;
  }

  template <class Parent>
  void handle_packet(Parent&, header_type&, span<const byte> payload) {
    binary_deserializer source{sys_, payload};
    message msg;
    if (auto err = msg.load(source))
      CAF_FAIL("unable to deserialize message: " << err);
    if (!msg.match_elements<std::string>())
      CAF_FAIL("unexpected message: " << msg);
    auto& str = msg.get_as<std::string>(0);
    auto bytes = as_bytes(make_span(str));
    buf_->insert(buf_->end(), bytes.begin(), bytes.end());
  }

  template <class Parent>
  void write_message(Parent& parent,
                     std::unique_ptr<endpoint_manager_queue::message> ptr) {
    // Ignore proxy announcement messages.
    if (ptr->msg == nullptr)
      return;
    auto header_buf = parent.next_header_buffer();
    binary_serializer sink{sys_, header_buf};
    header_type header{static_cast<uint32_t>(ptr->payload.size())};
    if (auto err = sink(header))
      CAF_FAIL("serializing failed: " << err);
    parent.write_packet(header_buf, ptr->payload);
  }

  static expected<std::vector<byte>> serialize(actor_system& sys,
                                               const type_erased_tuple& x) {
    std::vector<byte> result;
    binary_serializer sink{sys, result};
    if (auto err = message::save(sink, x))
      return err.value();
    return result;
  }

private:
  actor_system& sys_;
  std::shared_ptr<std::vector<byte>> buf_;
};

template <class Base, class Subtype>
class stream_string_application : public Base {
public:
  using header_type = typename Base::header_type;

  stream_string_application(actor_system& sys,
                            std::shared_ptr<std::vector<byte>> buf)
    : Base(sys, std::move(buf)), await_payload_(false) {
    // nop
  }

  template <class Parent>
  error init(Parent& parent) {
    parent.transport().configure_read(
      net::receive_policy::exactly(sizeof(header_type)));
    return Base::init(parent);
  }

  template <class Parent>
  error handle_data(Parent& parent, span<const byte> data) {
    if (await_payload_) {
      Base::handle_packet(parent, header_, data);
      await_payload_ = false;
    } else {
      if (data.size() != sizeof(header_type))
        CAF_FAIL("");
      binary_deserializer source{nullptr, data};
      if (auto err = source(header_))
        CAF_FAIL("serializing failed: " << err);
      if (header_.payload == 0)
        Base::handle_packet(parent, header_, span<const byte>{});
      else
        parent.transport().configure_read(
          net::receive_policy::exactly(header_.payload));
      await_payload_ = true;
    }
    return none;
  }

  template <class Parent>
  void resolve(Parent& parent, string_view path, actor listener) {
    actor_id aid = 42;
    auto hid = "0011223344556677889900112233445566778899";
    auto nid = unbox(make_node_id(aid, hid));
    actor_config cfg;
    auto sys = &parent.system();
    auto mgr = &parent.manager();
    auto p = make_actor<net::actor_proxy_impl, strong_actor_ptr>(aid, nid, sys,
                                                                 cfg, mgr);
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

  void handle_error(sec sec) {
    CAF_FAIL("handle_error called: " << CAF_ARG(sec));
  }

private:
  header_type header_;
  bool await_payload_;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(endpoint_manager_tests, fixture)

CAF_TEST(receive) {
  using application_type = extend<string_application>::with<
    stream_string_application>;
  using transport_type = stream_transport<application_type>;
  std::vector<byte> read_buf(1024);
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 1u);
  auto buf = std::make_shared<std::vector<byte>>();
  auto sockets = unbox(make_stream_socket_pair());
  nonblocking(sockets.second, true);
  CAF_CHECK_EQUAL(read(sockets.second, read_buf),
                  sec::unavailable_or_would_block);
  CAF_MESSAGE("adding both endpoint managers");
  auto mgr1 = make_endpoint_manager(mpx, sys,
                                    transport_type{sockets.first,
                                                   application_type{sys, buf}});
  CAF_CHECK_EQUAL(mgr1->init(), none);
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 2u);
  auto mgr2 = make_endpoint_manager(mpx, sys,
                                    transport_type{sockets.second,
                                                   application_type{sys, buf}});
  CAF_CHECK_EQUAL(mgr2->init(), none);
  CAF_CHECK_EQUAL(mpx->num_socket_managers(), 3u);
  CAF_MESSAGE("resolve actor-proxy");
  mgr1->resolve(unbox(make_uri("test:/id/42")), self);
  run();
  self->receive(
    [&](resolve_atom, const std::string&, const strong_actor_ptr& p) {
      CAF_MESSAGE("got a proxy, send a message to it");
      self->send(actor_cast<actor>(p), "hello proxy!");
    },
    after(std::chrono::seconds(0)) >>
      [&] { CAF_FAIL("manager did not respond with a proxy."); });
  run();
  CAF_CHECK_EQUAL(string_view(reinterpret_cast<char*>(buf->data()),
                              buf->size()),
                  "hello proxy!");
}

CAF_TEST_FIXTURE_SCOPE_END()
