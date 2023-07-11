// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/lower_layer.hpp"

#include "caf/error.hpp"
#include "caf/net/web_socket/status.hpp"
#include "caf/sec.hpp"

namespace caf::net::web_socket {

lower_layer::~lower_layer() {
  // nop
}

void lower_layer::shutdown() {
  using namespace std::literals;
  shutdown(status::normal_close, "EOF"sv);
}

void lower_layer::shutdown(const error& reason) {
  if (reason.code() == static_cast<uint8_t>(sec::connection_closed)) {
    shutdown(status::normal_close, to_string(reason));
  } else if (reason.code() == static_cast<uint8_t>(sec::protocol_error)) {
    shutdown(status::protocol_error, to_string(reason));
  } else if (reason.code() == static_cast<uint8_t>(sec::malformed_message)) {
    shutdown(status::inconsistent_data, to_string(reason));
  } else {
    shutdown(status::unexpected_condition, to_string(reason));
  }
}

} // namespace caf::net::web_socket
