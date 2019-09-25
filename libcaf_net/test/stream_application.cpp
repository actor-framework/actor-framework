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

#define CAF_SUITE basp.stream_application

#include "caf/net/basp/application.hpp"

#include "caf/test/dsl.hpp"

#include "host_fixture.hpp"

#include <vector>

#include "caf/byte.hpp"
#include "caf/forwarding_actor_proxy.hpp"
#include "caf/net/actor_proxy_impl.hpp"
#include "caf/net/basp/connection_state.hpp"
#include "caf/net/basp/constants.hpp"
#include "caf/net/basp/ec.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/none.hpp"
#include "caf/uri.hpp"

using namespace caf;
using namespace caf::net;

#define REQUIRE_OK(statement)                                                  \
  if (auto err = statement)                                                    \
    CAF_FAIL(sys.render(err));

namespace {

using buffer_type = std::vector<byte>;

using transport_type = stream_transport<basp::application>;

size_t fetch_size(variant<size_t, sec> x) {
  if (holds_alternative<sec>(x))
    CAF_FAIL("read/write failed: " << to_string(get<sec>(x)));
  return get<size_t>(x);
}

struct fixture : test_coordinator_fixture<>,
                 host_fixture,
                 proxy_registry::backend {
  fixture() {
    uri mars_uri;
    REQUIRE_OK(parse("tcp://mars", mars_uri));
    mars = make_node_id(mars_uri);
    mpx = std::make_shared<multiplexer>();
    if (auto err = mpx->init())
      CAF_FAIL("mpx->init failed: " << sys.render(err));
    auto proxies = std::make_shared<proxy_registry>(sys, *this);
    auto sockets = unbox(make_stream_socket_pair());
    sock = sockets.first;
    nonblocking(sockets.first, true);
    nonblocking(sockets.second, true);
    mgr = make_endpoint_manager(mpx, sys,
                                transport_type{sockets.second,
                                               basp::application{proxies}});
    REQUIRE_OK(mgr->init());
    mpx->handle_updates();
    CAF_CHECK_EQUAL(mpx->num_socket_managers(), 2u);
    auto& dref = dynamic_cast<endpoint_manager_impl<transport_type>&>(*mgr);
    app = &dref.application();
  }

  ~fixture() {
    close(sock);
  }

  bool handle_io_event() override {
    mpx->handle_updates();
    return mpx->poll_once(false);
  }

  template <class... Ts>
  buffer_type to_buf(const Ts&... xs) {
    buffer_type buf;
    serializer_impl<buffer_type> sink{system(), buf};
    REQUIRE_OK(sink(xs...));
    return buf;
  }

  template <class... Ts>
  void mock(const Ts&... xs) {
    auto buf = to_buf(xs...);
    if (fetch_size(write(sock, make_span(buf))) != buf.size())
      CAF_FAIL("unable to write " << buf.size() << " bytes");
    run();
  }

  void handle_handshake() {
    CAF_CHECK_EQUAL(app->state(),
                    basp::connection_state::await_handshake_header);
    auto payload = to_buf(mars, defaults::middleman::app_identifiers);
    mock(basp::header{basp::message_type::handshake,
                      static_cast<uint32_t>(payload.size()), basp::version});
    CAF_CHECK_EQUAL(app->state(),
                    basp::connection_state::await_handshake_payload);
    write(sock, make_span(payload));
    run();
  }

  void consume_handshake() {
    buffer_type buf(basp::header_size);
    if (fetch_size(read(sock, make_span(buf))) != basp::header_size)
      CAF_FAIL("unable to read " << basp::header_size << " bytes");
    auto hdr = basp::header::from_bytes(buf);
    if (hdr.type != basp::message_type::handshake || hdr.payload_len == 0
        || hdr.operation_data != basp::version)
      CAF_FAIL("invalid handshake header");
    buf.resize(hdr.payload_len);
    if (fetch_size(read(sock, make_span(buf))) != hdr.payload_len)
      CAF_FAIL("unable to read " << hdr.payload_len << " bytes");
    node_id nid;
    std::vector<std::string> app_ids;
    binary_deserializer source{sys, buf};
    if (auto err = source(nid, app_ids))
      CAF_FAIL("unable to deserialize payload: " << sys.render(err));
    if (source.remaining() > 0)
      CAF_FAIL("trailing bytes after reading payload");
  }

  actor_system& system() {
    return sys;
  }

  strong_actor_ptr make_proxy(node_id nid, actor_id aid) override {
    using impl_type = actor_proxy_impl;
    using hdl_type = strong_actor_ptr;
    actor_config cfg;
    return make_actor<impl_type, hdl_type>(aid, nid, &sys, cfg, mgr);
  }

  void set_last_hop(node_id*) override {
    // nop
  }

  node_id mars;

  multiplexer_ptr mpx;

  endpoint_manager_ptr mgr;

  stream_socket sock;

  basp::application* app;
};

} // namespace

#define MOCK(kind, op, ...)                                                    \
  do {                                                                         \
    auto payload = to_buf(__VA_ARGS__);                                        \
    mock(basp::header{kind, static_cast<uint32_t>(payload.size()), op});       \
    write(sock, make_span(payload));                                           \
    run();                                                                     \
  } while (false)

#define RECEIVE(msg_type, op_data, ...)                                        \
  do {                                                                         \
    buffer_type buf(basp::header_size);                                        \
    if (fetch_size(read(sock, make_span(buf))) != basp::header_size)           \
      CAF_FAIL("unable to read " << basp::header_size << " bytes");            \
    auto hdr = basp::header::from_bytes(buf);                                  \
    CAF_CHECK_EQUAL(hdr.type, msg_type);                                       \
    CAF_CHECK_EQUAL(hdr.operation_data, op_data);                              \
    buf.resize(hdr.payload_len);                                               \
    if (fetch_size(read(sock, make_span(buf))) != size_t{hdr.payload_len})     \
      CAF_FAIL("unable to read " << hdr.payload_len << " bytes");              \
    binary_deserializer source{sys, buf};                                      \
    if (auto err = source(__VA_ARGS__))                                        \
      CAF_FAIL("failed to receive data: " << sys.render(err));                 \
  } while (false)

CAF_TEST_FIXTURE_SCOPE(application_tests, fixture)

CAF_TEST(actor message) {
  handle_handshake();
  consume_handshake();
  sys.registry().put(self->id(), self);
  CAF_REQUIRE_EQUAL(self->mailbox().size(), 0u);
  MOCK(basp::message_type::actor_message, make_message_id().integer_value(),
       mars, actor_id{42}, self->id(), std::vector<strong_actor_ptr>{},
       make_message("hello world!"));
  allow((atom_value, strong_actor_ptr),
        from(_).to(self).with(atom("monitor"), _));
  expect((std::string), from(_).to(self).with("hello world!"));
}

CAF_TEST(resolve request without result) {
  handle_handshake();
  consume_handshake();
  CAF_CHECK_EQUAL(app->state(), basp::connection_state::await_header);
  MOCK(basp::message_type::resolve_request, 42, std::string{"foo/bar"});
  CAF_CHECK_EQUAL(app->state(), basp::connection_state::await_header);
  actor_id aid;
  std::set<std::string> ifs;
  RECEIVE(basp::message_type::resolve_response, 42u, aid, ifs);
  CAF_CHECK_EQUAL(aid, 0u);
  CAF_CHECK(ifs.empty());
}

CAF_TEST(resolve request on id with result) {
  handle_handshake();
  consume_handshake();
  sys.registry().put(self->id(), self);
  auto path = "id/" + std::to_string(self->id());
  CAF_CHECK_EQUAL(app->state(), basp::connection_state::await_header);
  MOCK(basp::message_type::resolve_request, 42, path);
  CAF_CHECK_EQUAL(app->state(), basp::connection_state::await_header);
  actor_id aid;
  std::set<std::string> ifs;
  RECEIVE(basp::message_type::resolve_response, 42u, aid, ifs);
  CAF_CHECK_EQUAL(aid, self->id());
  CAF_CHECK(ifs.empty());
}

CAF_TEST_FIXTURE_SCOPE_END()
