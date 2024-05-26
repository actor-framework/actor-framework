// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/web_socket/has_on_request.hpp"

namespace caf::net::web_socket {

has_on_request::~has_on_request() {
  sfb::release(config_);
}

dsl::server_config_value& has_on_request::base_config() {
  return sfb::upcast(*config_);
}

} // namespace caf::net::web_socket
