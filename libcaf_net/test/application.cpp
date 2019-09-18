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

#define CAF_SUITE basp.application

#include "caf/net/basp/application.hpp"

#include "caf/test/dsl.hpp"

#include <vector>

#include "caf/byte.hpp"
#include "caf/forwarding_actor_proxy.hpp"
#include "caf/net/basp/connection_state.hpp"
#include "caf/net/basp/constants.hpp"
#include "caf/net/basp/ec.hpp"
#include "caf/none.hpp"
#include "caf/uri.hpp"

using namespace caf;
using namespace caf::net;

#define REQUIRE_OK(statement)                                                  \
  if (auto err = statement)                                                    \
    CAF_FAIL("failed to serialize data: " << sys.render(err));

namespace {

struct fixture : test_coordinator_fixture<>, proxy_registry::backend {
  using buffer_type = std::vector<byte>;

  fixture() : app(std::make_shared<proxy_registry>(sys, *this)) {
    REQUIRE_OK(app.init(*this));
    uri mars_uri;
    REQUIRE_OK(parse("tcp://mars", mars_uri));
    mars = make_node_id(mars_uri);
  }

  template <class... Ts>
  buffer_type to_buf(const Ts&... xs) {
    buffer_type buf;
    serializer_impl<buffer_type> sink{system(), buf};
    REQUIRE_OK(sink(xs...));
    return buf;
  }

  template <class... Ts>
  void set_input(const Ts&... xs) {
    input = to_buf(xs...);
  }

  void write_packet(span<const byte> hdr, span<const byte> payload) {
    output.insert(output.end(), hdr.begin(), hdr.end());
    output.insert(output.end(), payload.begin(), payload.end());
  }

  void handle_handshake() {
    CAF_CHECK_EQUAL(app.state(),
                    basp::connection_state::await_handshake_header);
    auto payload = to_buf(mars, defaults::middleman::app_identifiers);
    set_input(basp::header{basp::message_type::handshake,
                           static_cast<uint32_t>(payload.size()),
                           basp::version});
    REQUIRE_OK(app.handle_data(*this, input));
    CAF_CHECK_EQUAL(app.state(),
                    basp::connection_state::await_handshake_payload);
    REQUIRE_OK(app.handle_data(*this, payload));
  }

  void consume_handshake() {
    if (output.size() < basp::header_size)
      CAF_FAIL("BASP application did not write a handshake header");
    auto hdr = basp::header::from_bytes(output);
    if (hdr.type != basp::message_type::handshake || hdr.payload_len == 0
        || hdr.operation_data != basp::version)
      CAF_FAIL("invalid handshake header");
    node_id nid;
    std::vector<std::string> app_ids;
    binary_deserializer source{sys, output};
    source.skip(basp::header_size);
    if (auto err = source(nid, app_ids))
      CAF_FAIL("unable to deserialize payload: " << sys.render(err));
    if (source.remaining() > 0)
      CAF_FAIL("trailing bytes after reading payload");
    output.clear();
  }

  actor_system& system() {
    return sys;
  }

  strong_actor_ptr make_proxy(node_id nid, actor_id aid) override {
    using impl_type = forwarding_actor_proxy;
    using hdl_type = strong_actor_ptr;
    actor_config cfg;
    return make_actor<impl_type, hdl_type>(aid, nid, &sys, cfg, self);
  }

  void set_last_hop(node_id*) override {
    // nop
  }

  buffer_type input;

  buffer_type output;

  node_id mars;

  basp::application app;
};

} // namespace

#define MOCK(kind, op, ...)                                                    \
  do {                                                                         \
    auto payload = to_buf(__VA_ARGS__);                                        \
    set_input(basp::header{kind, static_cast<uint32_t>(payload.size()), op});  \
    if (auto err = app.handle_data(*this, input))                              \
      CAF_FAIL("application-under-test failed to process header: "             \
               << sys.render(err));                                            \
    if (auto err = app.handle_data(*this, payload))                            \
      CAF_FAIL("application-under-test failed to process payload: "            \
               << sys.render(err));                                            \
  } while (false)

#define RECEIVE(msg_type, op_data, ...)                                        \
  do {                                                                         \
    binary_deserializer source{sys, output};                                   \
    basp::header hdr;                                                          \
    if (auto err = source(hdr, __VA_ARGS__))                                   \
      CAF_FAIL("failed to receive data: " << sys.render(err));                 \
    if (source.remaining() != 0)                                               \
      CAF_FAIL("unable to read entire message, " << source.remaining()         \
                                                 << " bytes left in buffer");  \
    CAF_CHECK_EQUAL(hdr.type, msg_type);                                       \
    CAF_CHECK_EQUAL(hdr.operation_data, op_data);                              \
    output.clear();                                                            \
  } while (false)

CAF_TEST_FIXTURE_SCOPE(application_tests, fixture)

CAF_TEST(missing handshake) {
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_handshake_header);
  set_input(basp::header{basp::message_type::heartbeat, 0, 0});
  CAF_CHECK_EQUAL(app.handle_data(*this, input), basp::ec::missing_handshake);
}

CAF_TEST(version mismatch) {
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_handshake_header);
  set_input(basp::header{basp::message_type::handshake, 0, 0});
  CAF_CHECK_EQUAL(app.handle_data(*this, input), basp::ec::version_mismatch);
}

CAF_TEST(missing payload in handshake) {
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_handshake_header);
  set_input(basp::header{basp::message_type::handshake, 0, basp::version});
  CAF_CHECK_EQUAL(app.handle_data(*this, input), basp::ec::missing_payload);
}

CAF_TEST(invalid handshake) {
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_handshake_header);
  node_id no_nid;
  std::vector<std::string> no_ids;
  auto payload = to_buf(no_nid, no_ids);
  set_input(basp::header{basp::message_type::handshake,
                         static_cast<uint32_t>(payload.size()), basp::version});
  REQUIRE_OK(app.handle_data(*this, input));
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_handshake_payload);
  CAF_CHECK_EQUAL(app.handle_data(*this, payload), basp::ec::invalid_handshake);
}

CAF_TEST(app identifier mismatch) {
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_handshake_header);
  std::vector<std::string> wrong_ids{"YOLO!!!"};
  auto payload = to_buf(mars, wrong_ids);
  set_input(basp::header{basp::message_type::handshake,
                         static_cast<uint32_t>(payload.size()), basp::version});
  REQUIRE_OK(app.handle_data(*this, input));
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_handshake_payload);
  CAF_CHECK_EQUAL(app.handle_data(*this, payload),
                  basp::ec::app_identifiers_mismatch);
}

CAF_TEST(repeated handshake) {
  handle_handshake();
  consume_handshake();
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_header);
  node_id no_nid;
  std::vector<std::string> no_ids;
  auto payload = to_buf(no_nid, no_ids);
  set_input(basp::header{basp::message_type::handshake,
                         static_cast<uint32_t>(payload.size()), basp::version});
  CAF_CHECK_EQUAL(app.handle_data(*this, input), none);
  CAF_CHECK_EQUAL(app.handle_data(*this, payload),
                  basp::ec::unexpected_handshake);
}

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
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_header);
  MOCK(basp::message_type::resolve_request, 42, std::string{"foo/bar"});
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_header);
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
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_header);
  MOCK(basp::message_type::resolve_request, 42, path);
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_header);
  actor_id aid;
  std::set<std::string> ifs;
  RECEIVE(basp::message_type::resolve_response, 42u, aid, ifs);
  CAF_CHECK_EQUAL(aid, self->id());
  CAF_CHECK(ifs.empty());
}

CAF_TEST(resolve request on name with result) {
  handle_handshake();
  consume_handshake();
  sys.registry().put(atom("foo"), self);
  std::string path = "name/foo";
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_header);
  MOCK(basp::message_type::resolve_request, 42, path);
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_header);
  actor_id aid;
  std::set<std::string> ifs;
  RECEIVE(basp::message_type::resolve_response, 42u, aid, ifs);
  CAF_CHECK_EQUAL(aid, self->id());
  CAF_CHECK(ifs.empty());
}

CAF_TEST(resolve response with invalid actor handle) {
  handle_handshake();
  consume_handshake();
  app.resolve(*this, "foo/bar", self);
  std::string path;
  RECEIVE(basp::message_type::resolve_request, 1u, path);
  CAF_CHECK_EQUAL(path, "foo/bar");
  actor_id aid = 0;
  std::set<std::string> ifs;
  MOCK(basp::message_type::resolve_response, 1u, aid, ifs);
  self->receive([&](strong_actor_ptr& hdl, std::set<std::string>& hdl_ifs) {
    CAF_CHECK_EQUAL(hdl, nullptr);
    CAF_CHECK_EQUAL(ifs, hdl_ifs);
  });
}

CAF_TEST(resolve response with valid actor handle) {
  handle_handshake();
  consume_handshake();
  app.resolve(*this, "foo/bar", self);
  std::string path;
  RECEIVE(basp::message_type::resolve_request, 1u, path);
  CAF_CHECK_EQUAL(path, "foo/bar");
  actor_id aid = 42;
  std::set<std::string> ifs;
  MOCK(basp::message_type::resolve_response, 1u, aid, ifs);
  self->receive([&](strong_actor_ptr& hdl, std::set<std::string>& hdl_ifs) {
    CAF_REQUIRE(hdl != nullptr);
    CAF_CHECK_EQUAL(ifs, hdl_ifs);
    CAF_CHECK_EQUAL(hdl->id(), aid);
  });
}

CAF_TEST(heartbeat message) {
  handle_handshake();
  consume_handshake();
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_header);
  auto bytes = to_bytes(basp::header{basp::message_type::heartbeat, 0, 0});
  set_input(bytes);
  REQUIRE_OK(app.handle_data(*this, input));
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_header);
}

CAF_TEST_FIXTURE_SCOPE_END()
