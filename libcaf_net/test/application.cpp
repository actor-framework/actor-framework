// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE basp.application

#include "caf/net/basp/application.hpp"

#include "caf/test/dsl.hpp"

#include <vector>

#include "caf/byte_buffer.hpp"
#include "caf/forwarding_actor_proxy.hpp"
#include "caf/net/basp/connection_state.hpp"
#include "caf/net/basp/constants.hpp"
#include "caf/net/basp/ec.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/packet_writer.hpp"
#include "caf/none.hpp"
#include "caf/uri.hpp"

using namespace caf;
using namespace caf::net;

#define REQUIRE_OK(statement)                                                  \
  if (auto err = statement)                                                    \
    CAF_FAIL("failed to serialize data: " << err);

namespace {

struct config : actor_system_config {
  config() {
    net::middleman::add_module_options(*this);
  }
};

struct fixture : test_coordinator_fixture<config>,
                 proxy_registry::backend,
                 basp::application::test_tag,
                 public packet_writer {
  fixture() : proxies(sys, *this), app(proxies) {
    REQUIRE_OK(app.init(*this));
    uri mars_uri;
    REQUIRE_OK(parse("tcp://mars", mars_uri));
    mars = make_node_id(mars_uri);
  }

  template <class... Ts>
  byte_buffer to_buf(const Ts&... xs) {
    byte_buffer buf;
    binary_serializer sink{system(), buf};
    REQUIRE_OK(sink(xs...));
    return buf;
  }

  template <class... Ts>
  void set_input(const Ts&... xs) {
    input = to_buf(xs...);
  }

  void handle_handshake() {
    CAF_CHECK_EQUAL(app.state(),
                    basp::connection_state::await_handshake_header);
    auto payload = to_buf(mars, basp::application::default_app_ids());
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
      CAF_FAIL("unable to deserialize payload: " << err);
    if (source.remaining() > 0)
      CAF_FAIL("trailing bytes after reading payload");
    output.clear();
  }

  actor_system& system() {
    return sys;
  }

  fixture& transport() {
    return *this;
  }

  endpoint_manager& manager() {
    CAF_FAIL("unexpected function call");
  }

  byte_buffer next_payload_buffer() override {
    return {};
  }

  byte_buffer next_header_buffer() override {
    return {};
  }

  template <class... Ts>
  void configure_read(Ts...) {
    // nop
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

protected:
  void write_impl(span<byte_buffer*> buffers) override {
    for (auto buf : buffers)
      output.insert(output.end(), buf->begin(), buf->end());
  }

  byte_buffer input;

  byte_buffer output;

  node_id mars;

  proxy_registry proxies;

  basp::application app;
};

} // namespace

#define MOCK(kind, op, ...)                                                    \
  do {                                                                         \
    auto payload = to_buf(__VA_ARGS__);                                        \
    set_input(basp::header{kind, static_cast<uint32_t>(payload.size()), op});  \
    if (auto err = app.handle_data(*this, input))                              \
      CAF_FAIL("application-under-test failed to process header: " << err);    \
    if (auto err = app.handle_data(*this, payload))                            \
      CAF_FAIL("application-under-test failed to process payload: " << err);   \
  } while (false)

#define RECEIVE(msg_type, op_data, ...)                                        \
  do {                                                                         \
    binary_deserializer source{sys, output};                                   \
    basp::header hdr;                                                          \
    if (auto err = source(hdr, __VA_ARGS__))                                   \
      CAF_FAIL("failed to receive data: " << err);                             \
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
  expect((monitor_atom, strong_actor_ptr), from(_).to(self));
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
  sys.registry().put("foo", self);
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
