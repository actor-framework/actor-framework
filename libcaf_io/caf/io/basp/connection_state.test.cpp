// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/basp/connection_state.hpp"

#include "caf/test/outline.hpp"

using namespace caf::io::basp;

namespace {

OUTLINE("critical connection states require a connection shutdown") {
  GIVEN("connection state <state>") {
    auto state = block_parameters<connection_state>();
    WHEN("calling require_shutdown with the state") {
      THEN("the result is <result>") {
        auto result = block_parameters<bool>();
        check_eq(requires_shutdown(state), result);
      }
    }
  }
  EXAMPLES = R"_(
    | state                           | result |
    | await_header                    | false  |
    | await_payload                   | false  |
    | close_connection                | true   |
    | incompatible_versions           | true   |
    | incompatible_application_ids    | true   |
    | malformed_message               | true   |
    | serializing_basp_payload_failed | true   |
    | redundant_connection            | true   |
    | no_route_to_receiving_node      | true   |
  )_";
}

OUTLINE("connection states are convertible to system error codes") {
  GIVEN("connection state <state>") {
    auto state = block_parameters<connection_state>();
    WHEN("calling to_sec with the state") {
      THEN("the result is <result>") {
        auto result = block_parameters<caf::sec>();
        check_eq(to_sec(state), result);
      }
    }
  }
  EXAMPLES = R"_(
    | state                           | result                            |
    | await_header                    | none                              |
    | await_payload                   | none                              |
    | close_connection                | none                              |
    | incompatible_versions           | incompatible_versions             |
    | incompatible_application_ids    | incompatible_application_ids      |
    | malformed_message               | malformed_message                 |
    | serializing_basp_payload_failed | serializing_basp_payload_failed   |
    | redundant_connection            | redundant_connection              |
    | no_route_to_receiving_node      | no_route_to_receiving_node        |
  )_";
}

} // namespace
