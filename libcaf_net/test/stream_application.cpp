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

#include "caf/net/test/host_fixture.hpp"
#include "caf/test/dsl.hpp"

#include <vector>

#include "caf/net/backend/test.hpp"
#include "caf/net/basp/connection_state.hpp"
#include "caf/net/basp/constants.hpp"
#include "caf/net/basp/ec.hpp"
#include "caf/net/make_endpoint_manager.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/uri.hpp"

using namespace caf;
using namespace caf::net;

#define REQUIRE_OK(statement)                                                  \
  if (auto err = statement)                                                    \
    CAF_FAIL(err);

namespace {

using transport_type = stream_transport<basp::application>;

size_t fetch_size(variant<size_t, sec> x) {
  if (holds_alternative<sec>(x))
    CAF_FAIL("read/write failed: " << to_string(get<sec>(x)));
  return get<size_t>(x);
}

struct config : actor_system_config {
  config() {
    put(content, "middleman.this-node", unbox(make_uri("test:earth")));
    load<middleman, backend::test>();
  }
};

struct fixture : host_fixture, test_coordinator_fixture<config> {
  fixture() : mars(make_node_id(unbox(make_uri("test:mars")))) {
    auto& mm = sys.network_manager();
    mm.mpx()->set_thread_id();
    auto backend = dynamic_cast<backend::test*>(mm.backend("test"));
    auto mgr = backend->peer(mars);
    auto& dref = dynamic_cast<endpoint_manager_impl<transport_type>&>(*mgr);
    app = &dref.transport().application();
    sock = backend->socket(mars);
  }

  bool handle_io_event() override {
    auto mpx = sys.network_manager().mpx();
    return mpx->poll_once(false);
  }

  template <class... Ts>
  byte_buffer to_buf(const Ts&... xs) {
    byte_buffer buf;
    binary_serializer sink{system(), buf};
    REQUIRE_OK(sink(xs...));
    return buf;
  }

  template <class... Ts>
  void mock(const Ts&... xs) {
    auto buf = to_buf(xs...);
    if (fetch_size(write(sock, buf)) != buf.size())
      CAF_FAIL("unable to write " << buf.size() << " bytes");
    run();
  }

  void handle_handshake() {
    CAF_CHECK_EQUAL(app->state(),
                    basp::connection_state::await_handshake_header);
    auto payload = to_buf(mars, basp::application::default_app_ids());
    mock(basp::header{basp::message_type::handshake,
                      static_cast<uint32_t>(payload.size()), basp::version});
    CAF_CHECK_EQUAL(app->state(),
                    basp::connection_state::await_handshake_payload);
    write(sock, payload);
    run();
  }

  void consume_handshake() {
    byte_buffer buf(basp::header_size);
    if (fetch_size(read(sock, buf)) != basp::header_size)
      CAF_FAIL("unable to read " << basp::header_size << " bytes");
    auto hdr = basp::header::from_bytes(buf);
    if (hdr.type != basp::message_type::handshake || hdr.payload_len == 0
        || hdr.operation_data != basp::version)
      CAF_FAIL("invalid handshake header");
    buf.resize(hdr.payload_len);
    if (fetch_size(read(sock, buf)) != hdr.payload_len)
      CAF_FAIL("unable to read " << hdr.payload_len << " bytes");
    node_id nid;
    std::vector<std::string> app_ids;
    binary_deserializer source{sys, buf};
    if (auto err = source(nid, app_ids))
      CAF_FAIL("unable to deserialize payload: " << err);
    if (source.remaining() > 0)
      CAF_FAIL("trailing bytes after reading payload");
  }

  actor_system& system() {
    return sys;
  }

  node_id mars;

  stream_socket sock;

  basp::application* app;

  unit_t no_payload;
};

} // namespace

#define MOCK(kind, op, ...)                                                    \
  do {                                                                         \
    CAF_MESSAGE("mock " << kind);                                              \
    if (!std::is_same<decltype(std::make_tuple(__VA_ARGS__)),                  \
                      std::tuple<unit_t>>::value) {                            \
      auto payload = to_buf(__VA_ARGS__);                                      \
      mock(basp::header{kind, static_cast<uint32_t>(payload.size()), op});     \
      write(sock, payload);                                                    \
    } else {                                                                   \
      mock(basp::header{kind, 0, op});                                         \
    }                                                                          \
    run();                                                                     \
  } while (false)

#define RECEIVE(msg_type, op_data, ...)                                        \
  do {                                                                         \
    CAF_MESSAGE("receive " << msg_type);                                       \
    byte_buffer buf(basp::header_size);                                        \
    if (fetch_size(read(sock, buf)) != basp::header_size)                      \
      CAF_FAIL("unable to read " << basp::header_size << " bytes");            \
    auto hdr = basp::header::from_bytes(buf);                                  \
    CAF_CHECK_EQUAL(hdr.type, msg_type);                                       \
    CAF_CHECK_EQUAL(hdr.operation_data, op_data);                              \
    if (!std::is_same<decltype(std::make_tuple(__VA_ARGS__)),                  \
                      std::tuple<unit_t>>::value) {                            \
      buf.resize(hdr.payload_len);                                             \
      if (fetch_size(read(sock, buf)) != size_t{hdr.payload_len})              \
        CAF_FAIL("unable to read " << hdr.payload_len << " bytes");            \
      binary_deserializer source{sys, buf};                                    \
      if (auto err = source(__VA_ARGS__))                                      \
        CAF_FAIL("failed to receive data: " << err);                           \
    } else {                                                                   \
      if (hdr.payload_len != 0)                                                \
        CAF_FAIL("unexpected payload");                                        \
    }                                                                          \
  } while (false)

CAF_TEST_FIXTURE_SCOPE(application_tests, fixture)

CAF_TEST(actor message and down message) {
  handle_handshake();
  consume_handshake();
  sys.registry().put(self->id(), self);
  CAF_REQUIRE_EQUAL(self->mailbox().size(), 0u);
  MOCK(basp::message_type::actor_message, make_message_id().integer_value(),
       mars, actor_id{42}, self->id(), std::vector<strong_actor_ptr>{},
       make_message("hello world!"));
  MOCK(basp::message_type::monitor_message, 42u, no_payload);
  strong_actor_ptr proxy;
  self->receive([&](const std::string& str) {
    CAF_CHECK_EQUAL(str, "hello world!");
    proxy = self->current_sender();
    CAF_REQUIRE_NOT_EQUAL(proxy, nullptr);
    self->monitor(proxy);
  });
  MOCK(basp::message_type::down_message, 42u,
       error{exit_reason::user_shutdown});
  expect((down_msg),
         from(_).to(self).with(down_msg{actor_cast<actor_addr>(proxy),
                                        exit_reason::user_shutdown}));
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
