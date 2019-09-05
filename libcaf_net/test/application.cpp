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

struct fixture : test_coordinator_fixture<> {
  using buffer_type = std::vector<byte>;

  fixture() {
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

  void handle_magic_number() {
    CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_magic_number);
    set_input(basp::magic_number);
    REQUIRE_OK(app.handle_data(*this, input));
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

  actor_system& system() {
    return sys;
  }

  buffer_type input;

  buffer_type output;

  node_id mars;

  basp::application app;
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(application_tests, fixture)

CAF_TEST(invalid magic number) {
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_magic_number);
  set_input(basp::magic_number + 1);
  CAF_CHECK_EQUAL(app.handle_data(*this, input),
                  basp::ec::invalid_magic_number);
}

CAF_TEST(missing handshake) {
  handle_magic_number();
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_handshake_header);
  set_input(basp::header{basp::message_type::heartbeat, 0, 0});
  CAF_CHECK_EQUAL(app.handle_data(*this, input), basp::ec::missing_handshake);
}

CAF_TEST(version mismatch) {
  handle_magic_number();
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_handshake_header);
  set_input(basp::header{basp::message_type::handshake, 0, 0});
  CAF_CHECK_EQUAL(app.handle_data(*this, input), basp::ec::version_mismatch);
}

CAF_TEST(missing payload in handshake) {
  handle_magic_number();
  CAF_CHECK_EQUAL(app.state(), basp::connection_state::await_handshake_header);
  set_input(basp::header{basp::message_type::handshake, 0, basp::version});
  CAF_CHECK_EQUAL(app.handle_data(*this, input), basp::ec::missing_payload);
}

CAF_TEST(invalid handshake) {
  handle_magic_number();
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
  handle_magic_number();
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

CAF_TEST_FIXTURE_SCOPE_END()
